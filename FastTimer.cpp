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

