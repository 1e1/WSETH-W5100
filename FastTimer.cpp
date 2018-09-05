#include "FastTimer.h"




/***********************************************************
 *                       PROPERTIES                        *
 **********************************************************/




EthernetUDP FastTimer::_server;
IPAddress FastTimer::_timeServer(NTP_SERVER);
uint8_t FastTimer::_embedTime      = -1; // force "new time section" at startup
uint8_t FastTimer::_referenceTime  = B01111111;




/***********************************************************
 *                         PUBLIC                          *
 **********************************************************/




void FastTimer::begin()
{
  _server.begin(NTP_LOCALPORT);
}


/**
  *   0: same time section
  *   1: new time section
  * 255: new time section, new cycle
  *
  * @return int8_t
  */
const uint8_t FastTimer::update()
{
  const uint8_t previousTime  = FastTimer::_embedTime;
  /** located in macros.h, config.h define EMBEDTIME as:
  #define EMBEDTIME_1s_4m    ((uint8_t) (millis() >> 10))     # (2^10)/1024=1  |  1*(2^8) :   256s =  4m16s
  #define EMBEDTIME_2s_8m    ((uint8_t) (millis() >> 11))     # (2^11)/1024=2  |  2*(2^8) :   512s =  8m32s
  #define EMBEDTIME_4s_15m   ((uint8_t) (millis() >> 12))     # (2^12)/1024=4  |  4*(2^8) :  1024s =  17m04s
  #define EMBEDTIME_8s_30m   ((uint8_t) (millis() >> 13))     # (2^13)/1024=8  |  8*(2^8) :  2048s =  34m08s
  #define EMBEDTIME_16s_1h   ((uint8_t) (millis() >> 14))     # (2^14)/1024=16 | 16*(2^8) :  4096s = 1h08m16s
  #define EMBEDTIME_32s_2h   ((uint8_t) (millis() >> 15))     # (2^15)/1024=32 | 32*(2^8) :  8192s = 2h16m32s
  #define EMBEDTIME_64s_4h   ((uint8_t) (millis() >> 16))     # (2^16)/1024=64 | 64*(2^8) : 16384s = 4h33m04s
  #define EMBEDTIME_1s_18h   ((unsigned int) (millis() >> 10))# (2^10)/1024=1  | 1*(2^16) :  65536s =  18h12m16s
  #define EMBEDTIME_2s_36h   ((unsigned int) (millis() >> 11))# (2^11)/1024=2  | 2*(2^16) : 131072s =  1d12h24m32s
  #define EMBEDTIME_4s_3d    ((unsigned int) (millis() >> 12))# (2^12)/1024=4  | 4*(2^16) : 262144s =  3d00h49m04s
  **/
  FastTimer::_embedTime      = EMBEDTIME;
  return FastTimer::_embedTime ^ previousTime;
}


void FastTimer::requestNtp()
{
  // http://tools.ietf.org/html/rfc1305
  byte ntp_packet[NTP_PACKET_SIZE] = {
    // LI = 11, alarm condition (FastTimer not synchronized)
    // VN = 011, Version Number: currently 3
    // VM = 011, client
    // Stratum = 0, unspecified
    // Poll Interval: 6 = 64 seconds
    // Precision: 16MHz Arduino is about 2**-24
    B11100011, 0, 6, -24,
    // Root Delay: 29s -> target less than 1 minute (64s)
    0, 29, 0, 0,
    // Root Dispersion: 29s -> target less than 1 minute (64s)
    0, 29, 0, 0,
    // Reference FastTimer Identifier
    IP //0, 0, 0, DEVICE_NUMBER,
    //0
  };
  FastTimer::_server.beginPacket(FastTimer::_timeServer, NTP_PORT);
  /*
  for (uint8_t i=0; i<NTP_PACKET_SIZE; i++) {
    server.write(pgm_read_byte_near(&ntp_packet[i]));
  }
  */
  FastTimer::_server.write(ntp_packet, NTP_PACKET_SIZE);
  FastTimer::_server.endPacket();
}


const boolean FastTimer::readNtp()
{
  const uint8_t previousTime = FastTimer::_referenceTime;
  unsigned long /* seconds|iDay */xSince1900;
  uint8_t dst = B0, dayOfWeek = B111, hour = B1111;
  byte ntp_packet[NTP_PACKET_SIZE];

#if TZ_DST
  int dayOfYear;
  uint8_t yearSince1900, iLeapYear, deltaDays;
#endif TZ_DST

  if (FastTimer::_server.parsePacket()) {
    FastTimer::_server.read(ntp_packet, NTP_PACKET_SIZE);
    /*seconds*/xSince1900 = long(ntp_packet[40]) << 24
                          | long(ntp_packet[41]) << 16
                          | int(ntp_packet[42]) << 8
                          | int(ntp_packet[43]) << 0
                          ;
    /*seconds*/xSince1900+= TZ_OFFSET *3600;
    hour                = (/*seconds*/xSince1900 / 3600) % 24;
    /*iDay*/xSince1900  = /*seconds*/xSince1900 / 86400L;
    dayOfWeek           = (MONDAY + /*iDay*/xSince1900) % 7;

#if TZ_DST
    iLeapYear           = ( (/*iDay*/xSince1900 /* first days of 1900 */ -31 -28) / (365*4 +1) );
    yearSince1900       = (/*iDay*/xSince1900 - iLeapYear) / 365;
    dayOfYear           = 1 + ((/*iDay*/xSince1900 - iLeapYear) % 365);
    //dayOfYear           = 1 + ((/*iDay*/xSince1900 - iLeapYear) - (365 * yearSince1900));

    // http://www.legifrance.gouv.fr/affichTexte.do?cidTexte=JORFTEXT000000221946&dateTexte=&categorieLien=id
    //( ? + year + int(year/4) /* Arduino will die before ( */ - int(year/100) + int(year/400) /* ) */ ) % 7;
    // work until 2104 = 1900 + 204
    // 255 = (204 + 204/4)
    deltaDays = (yearSince1900 + iLeapYear) /* % 7 */;
    dayOfYear-= (31 + 28 + 31);
    if ((yearSince1900 % 4) == 0) {
      dayOfYear--;
    }

    // last sunday of march = 31 - ( (SATURDAY + deltaDays) % 7 );
    if (dayOfYear > ((SATURDAY + deltaDays) % 7)) {

      dayOfYear-= (30 + 31 + 30 + 31 + 31 + 30 + 31);
      // last sunday of october = 31 - ( (WEDNESDAY  + deltaDays) % 7 );
      if (dayOfYear < ((WEDNESDAY + deltaDays) % 7)) {

        // DST days
        dst = B1;
        hour++;

      } else if (dayOfYear == ((WEDNESDAY + deltaDays) % 7)) {

        // last DST day
        if (hour < byte(2) ) {
          dst = B1;
          hour++;
        }

      }

    } else if (dayOfYear == ((SATURDAY + deltaDays) % 7)) {

      // first DST day
      if (hour >= byte(2)) {
        dst = B1;
        hour++;
      }

    }

    if (hour==24) {
      //dayOfYear++;
      ///*seconds*/xSince1900+= 3600;
      dayOfWeek = (dayOfWeek + 1) %7;
    }
#endif TZ_DST

    FastTimer::_referenceTime = FMASK_DST(dst) | FMASK_DAY(dayOfWeek) | FMASK_HOUR(hour);
    LOG("DST="); LOG(dst); LOG("; DAY="); LOG(dayOfWeek); LOG("; HOUR="); LOGLN(hour);
  }
  FastTimer::_server.flush();
  return previousTime ^ FastTimer::_referenceTime;
}

