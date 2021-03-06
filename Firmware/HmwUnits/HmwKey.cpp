#include "HmwKey.h"
#include "HmwDevice.h"

// Class HmwKey
HmwKey::HmwKey( PortPin _pin, Config* _config, HmwChannel* _feedbackChannel ) :
   unlocked( true ),
   needsPulldownIfInverted( false ),
   keyPressNum( 0 ),
   config( _config ),
   feedbackChannel( _feedbackChannel ),
   digitalIn( _pin )
{
   type = HmwChannel::HMW_KEY;
   resetChannel();
}


void HmwKey::loop( uint8_t channel )
{
   if ( isUnlocked() )
   {
      if ( config->isPushButton() )
      {
         handlePushButtonSignal( channel );
      }
      else if ( config->isSwitch() )
      {
         handleSwitchSignal( channel );
      }
   }
}

void HmwKey::handleSwitchSignal( uint8_t channel )
{
   if ( !isPressed() )
   {
      if ( lastSentLong.isValid() )
      {
         // if return value is 1, bus is not idle, retry next time
         if ( HmwDevice::sendKeyEvent( channel, keyPressNum, false ) == IStream::SUCCESS )
         {
            keyPressNum++;
            lastSentLong.reset();
         }
      }
      keyPressedTimestamp.reset();
      if ( feedbackChannel && config->isFeedbackEnabled() )
      {
         uint8_t cmd = KEY_FEEDBACK_OFF;
         feedbackChannel->set( 1, &cmd );
      }
   }
   else
   {
      if ( !keyPressedTimestamp.isValid() )
      {
         // Taste war vorher nicht gedrueckt
         keyPressedTimestamp = Timestamp();
      }
      else if ( ( keyPressedTimestamp.since() >= 100 ) && !lastSentLong.isValid() )
      {
         // if return value is 1, bus is not idle, retry next time
         if ( HmwDevice::sendKeyEvent( channel, keyPressNum, false ) == IStream::SUCCESS )
         {
            keyPressNum++;
            lastSentLong = Timestamp();
         }
      }
      if ( feedbackChannel && config->isFeedbackEnabled() )
      {
         uint8_t cmd = KEY_FEEDBACK_ON;
         feedbackChannel->set( 1, &cmd );
      }
   }
}

void HmwKey::handlePushButtonSignal( uint8_t channel )
{
   if ( !isPressed() )
   {
      // d.h. Taste nicht gedrueckt
      // "Taste war auch vorher nicht gedrueckt" kann ignoriert werden
      // Taste war vorher gedrueckt?
      if ( keyPressedTimestamp.isValid() )
      {
         // entprellen, nur senden, wenn laenger als 50ms gedrueckt
         if ( ( keyPressedTimestamp.since() >= 50 ) )
         {
            if ( !lastSentLong.isValid() )
            {
               // noch kein "long" gesendet, f�r kurzes dr�cken keyPressNum erh�hen
               keyPressNum++;
            }
            // auch beim loslassen nach einem langen Tastendruck ein weiteres Event senden
            HmwDevice::sendKeyEvent( channel, keyPressNum, lastSentLong.isValid() );
         }
         keyPressedTimestamp.reset();
         if ( feedbackChannel && config->isFeedbackEnabled() )
         {
            uint8_t cmd = KEY_FEEDBACK_OFF;
            feedbackChannel->set( 1, &cmd );
         }
      }
   }
   else
   {
      // Taste gedrueckt
      // Taste war vorher schon gedrueckt
      if ( keyPressedTimestamp.isValid() )
      {
         // muessen wir ein "long" senden?
         if ( lastSentLong.isValid() )
         {
            // schon ein LONG gesendet
            if ( ( lastSentLong.since() >= 300 ) && config->repeatOnLongPress() )
            {
               // alle 300ms wiederholen
               // keyPressNum nicht erhoehen
               lastSentLong = Timestamp();
               HmwDevice::sendKeyEvent( channel, keyPressNum, true, true );                  // long press
            }
         }
         else if ( keyPressedTimestamp.since() >= long(config->getLongPressTime() ) * 100 )
         {
            // erstes LONG
            keyPressNum++;
            lastSentLong = Timestamp();
            HmwDevice::sendKeyEvent( channel, keyPressNum, true, true );                    // long press
         }
      }
      else
      {
         // Taste war vorher nicht gedrueckt
         keyPressedTimestamp = Timestamp();
         lastSentLong.reset();
         if ( feedbackChannel && config->isFeedbackEnabled() )
         {
            uint8_t cmd = KEY_FEEDBACK_ON;
            feedbackChannel->set( 1, &cmd );
         }
      }
   }
}

void HmwKey::resetChannel()
{
   if ( config->isActiveLow() )
   {
      digitalIn.setInverted( false );
      digitalIn.enablePullup();
   }
   else
   {
      digitalIn.setInverted( true );
      needsPulldownIfInverted ? digitalIn.enablePulldown() : digitalIn.enablePullup();
   }
   keyPressedTimestamp.reset();
   lastSentLong.reset();
   keyPressNum = 0;
}

void HmwKey::checkConfig()
{
   if ( ( config->getLongPressTime() < 4 ) || ( config->getLongPressTime() > 50 ) )
   {
      config->setLongPressTime( Config::DEFAULT_LONG_PRESS_TIME );
   }
   if ( config->getInputType() >= Config::MAX_INPUTTYPE )
   {
      config->setInputType( Config::PUSHBUTTON );
   }
   resetChannel();
}