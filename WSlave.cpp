#include "WSlave.h"




/***********************************************************
 *                       PROPERTIES                        *
 **********************************************************/




EthernetServer WSlave::_server(PORT);
EthernetClient WSlave::_client;

LONGSTRING(header_200)    = "200 OK";
LONGSTRING(header_401)    = "401 Authorization Required" CRLF "WWW-Authenticate: Basic realm=\"" DEVICE_NAME "\"";
LONGSTRING(header_417)    = "417 Expectation failed";
LONGSTRING(header_text)   = "text/plain";
LONGSTRING(header_json)   = "application/json";
LONGSTRING(header_htZ)    = "text/html" CRLF "Content-Encoding: gzip";
LONGSTRING(header_end)    = CRLF CRLF;
LONGSTRING(crlf)          = CRLF;

LONGSTRING(email)         = EMAIL;

LONGBYTES(webpage)        = WEBPAGE;
static size_t webpage_len = ARRAYLEN(webpage); // ~ 1557o / 1600o / 1709o / 2100o

LONGSTRING(json_qcolon)   = "\":\"";
LONGSTRING(json_qcomma)   = "\",\"";
LONGSTRING(json_qbrace1)  = "{\"";
LONGSTRING(json_qbrace2)  = "\"}";




/***********************************************************
 *                         PUBLIC                          *
 **********************************************************/




void WSlave::begin()
{
  byte mac[] = { MAC };
  const IPAddress ip(IP);
  /*
  const IPAddress dns(DNS);
  const IPAddress gateway(GATEWAY);
  const IPAddress subnet(SUBNET);
  */
#if USE_DHCP
  LOGLN("Trying to get an IP address using DHCP");
  if (0==Ethernet.begin(mac)) {
    LOGLN("Failed to configure Ethernet using DHCP");
#endif
    Ethernet.begin(mac, ip/*, dns, gateway, subnet*/);
#if USE_DHCP
  }
#endif
  // then don't forget Ethernet.maintain()
  LOG("IP:   ");  LOGLN(Ethernet.localIP());
  LOG("MASK: ");  LOGLN(Ethernet.subnetMask());
  LOG("GATE: ");  LOGLN(Ethernet.gatewayIP());
  LOG("DNS:  ");  LOGLN(Ethernet.dnsServerIP());
  LOG("listen "); LOGLN(PORT);
  WSlave::_server.begin();
#if USE_BONJOUR
  EthernetBonjour.begin(DEVICE_NAME);
  EthernetBonjour.addServiceRecord(DEVICE_NAME "._http", PORT, MDNSServiceTCP);
#endif USE_BONJOUR
  WSlave::openEmail(PSTR("START"));
  WSlave::closeEmail();
}


void WSlave::check()
{
  if (WSlave::_client = _server.available()) {
    LOGLN(">>> ETH0");

    MethodType method       = INVALID;
    ActionType action       = ROOT;
    uint8_t watchdog        = MAXHEADERS;
    boolean isUnauthorized  = true;

    Core2::setStream(&(WSlave::_client));

    // Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
    Core2::readUntil(SP);
    if (Core2::bufferIsEqualTo_P(PSTR("GET"))) {
      LOG("GET ");
      method = GET;
    } else if (Core2::bufferIsEqualTo_P(PSTR("PUT"))) {
      LOG("PUT ");
      method = PUT;
    }/* else if (Core2::bufferIsEqualTo_P(PSTR("POST"))) {
      LOG("POST ");
      method = POST;
    }*/ else goto _send;

    Core2::readUntil(SP);
    if (Core2::bufferIsPrefixOf_P(PSTR("/ws"))) {
      action = SERVICE;
      LOGLN("webservice");
      // TODO catch /ws?param!!!
    } else if (Core2::bufferIsPrefixOf_P(PSTR("/dict"))) {
      action = DICTIONARY;
      LOGLN("dictionary");
    }
    WSlave::lineLength(); // ends first Header line

    // check credentials = Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
    //header_401
    do {
      Core2::readUntil(SP);
      if (Core2::bufferIsEqualTo_P(PSTR("Authorization:"))) {
        Core2::readUntil(SP);
        if (Core2::bufferIsEqualTo_P(PSTR("Basic"))) {
          Core2::readUntil(CR);
          if (Core2::bufferIsEqualTo_P(PSTR(HTTP_AUTH64))) {
            isUnauthorized = false;
            WSlave::lineLength(); // ends first Header line
            goto _crlfcrlf;
          }
        }
      }
    } while(isUnauthorized && WSlave::lineLength()>1 && --watchdog);

    // sweep headers until CRLF CRLF
    _crlfcrlf:
//    while (WSlave::nextHttpLine() && --watchdog);
    while (WSlave::lineLength()>1 && --watchdog);
    if (!watchdog) {
      LOGLN("INVALID");
      method = INVALID;
    }

    // on body:
    if (method == PUT && action == SERVICE) {
      LOGLN("reading body");
      // [0-9]+=[0-9]+(&[0-9]+=[0-9]+)*
      Core2::processLine();
      //Core2::readUntil(CR);
      //Core2::printBuffer();
    }
    LOG("isUnauthorized="); LOGLN(isUnauthorized);
    LOG("watchdog="); LOGLN(watchdog);

    _send:
    if (isUnauthorized) {
      WSlave::sendHeaders_P(header_401, header_text);
    } else {
      if (method == INVALID) {
        WSlave::sendHeaders_P(header_417, header_text);
      } else {
        switch (action) {
          case SERVICE:
          WSlave::sendHeaders_P(header_200, header_json);
          LOGLN("< send service");
          WSlave::sendService();
          break;
          case DICTIONARY:
          WSlave::sendHeaders_P(header_200, header_json);
          LOGLN("< send dictionnary");
          WSlave::sendDictionary();
          break;
          default:
          LOG("< webpage_len="); LOGLN(webpage_len);
          WSlave::sendHeaders_P(header_200, header_htZ);
          WSlave::sendBody_P(webpage, webpage_len);
        } // switch (action)
      } // else (method == INVALID)
    } // else isUnauthorized
    LOGLN("<<< ETH0");
  } // if (WSlave::_client = _server.available())
}


void WSlave::openEmail(const prog_char* subject)
{
  byte smtp[]       = { SMTP_IP };
  uint8_t watchdog  = MAXRETRIES;
  uint8_t state     = 5;
  Core2::setStream(&(WSlave::_client));
  LOGLN("BEGIN");
  if (WSlave::_client.connect(smtp, SMTP_PORT)) {
    do {
      WSlave::waitClient(watchdog);
      LOG("< state #");
      switch (--state) {
        case 4:
        Core2::copyToBuffer_P(PSTR("HELO")); // HELO | EHLO
        break;
        case 3:
        Core2::copyToBuffer_P(PSTR("MAIL FROM:"));
        Core2::copyToBuffer_P( email );
        break;
        case 2:
        Core2::copyToBuffer_P(PSTR("RCPT TO:"));
        Core2::copyToBuffer_P( email );
        break;
        case 1:
        Core2::copyToBuffer_P(PSTR("DATA"));
        break;
        case 0:
        Core2::copyToBuffer_P(PSTR("Subject:" ML_SUBJECT));
        Core2::copyToBuffer_P(subject);
        Core2::sendBufferLn();
        break;
      }
      Core2::sendBufferLn();
      LOGLN(state);
    } while(watchdog && state);
  }
}


void WSlave::closeEmail()
{
  uint8_t watchdog  = MAXRETRIES;
  /**uint8_t state     = 2;**/
  //Core2::setStream(&(WSlave::_client));
  /**do {
    LOG("< state #");
    switch (--state) {
      case 1:**/
      Core2::sendBufferLn();
      Core2::copyToBuffer('.');
      /**break;
      case 0:
      Core2::copyToBuffer_P(PSTR("QUIT"));
      break;
    }**/
    Core2::sendBufferLn();
    /**LOGLN(state);**/
    WSlave::waitClient(watchdog);
  /**} while(/*watchdog && * /state);**/
  WSlave::_client.stop();
  LOGLN("END");
}




/***********************************************************
 *                        PROTECTED                        *
 **********************************************************/




/**
  * Status:
  *   1: 200
  *   2: 200
  *   3: 200
  *   *: 400 "Bad Request"
  *   *: 417 "The behavior expected fot the server is not supported."
  * Content-Type:
  *   1: application/json
  *   2: text/html
  *   3: text/cache-manifest
  * Content-Encoding:
  *   2: gzip
  * Cache-Control:
  *   2: max-age=604800 // 7* 24* 60* 60
  * Connection: close
  */
void WSlave::sendHeaders_P(const prog_char* codeStatus, const prog_char* contentType)
{
  Core2::unbuffer();
  Core2::copyToBuffer_P(PSTR("HTTP/1.0 ")); // 1.0 = auto close
  Core2::copyToBuffer_P(codeStatus);
  Core2::copyToBuffer_P(PSTR(CRLF "Content-Type: "));
  Core2::copyToBuffer_P(contentType);
  //Core2::copyToBuffer_P(PSTR(CRLF "Connection: close"));
  Core2::copyToBuffer_P(header_end);
  Core2::sendBuffer();
}


void WSlave::sendDictionary()
{
  uint8_t comma = Core2::total_len;
  Core2::unbuffer();
  Core2::copyToBuffer_P(json_qbrace1);
  // schedules
  for (uint8_t i=0; i < Core2::schedules_len; i++) {
    WSlave::sendToJson('S', Core2::schedules[i], --comma);
  }
  // pulses
  for (uint8_t i=0; i < Core2::pulses_len; i++) {
    WSlave::sendToJson('P', Core2::pulses[i], --comma);
  }
  // digitals
  for (uint8_t i=0; i < Core2::digitals_len; i++) {
    WSlave::sendToJson('D', Core2::digitals[i], --comma);
  }
  /*
  // messages
  for (uint8_t i=0; i < Core2::messages_len; i++) {
    WSlave::sendToJson('M', Core2::messages[i], --comma);
  }
  */
  Core2::copyToBuffer_P(json_qbrace2);
  Core2::sendBuffer();
}


void WSlave::sendService()
{
  uint8_t comma = Core2::total_len;
  Core2::unbuffer();
  Core2::copyToBuffer('[');
  // schedules
  for (uint8_t i=0; i < Core2::schedules_len; i++) {
    Core2::copyToBuffer(Core2::schedules[i].getValue());
    if (--comma) {
      Core2::copyToBuffer(',');
    }
  }
  // pulses
  for (uint8_t i=0; i < Core2::pulses_len; i++) {
    Core2::copyToBuffer(Core2::pulses[i].getValue());
    if (--comma) {
      Core2::copyToBuffer(',');
    }
  }
  // digitals
  for (uint8_t i=0; i < Core2::digitals_len; i++) {
    Core2::copyToBuffer(Core2::digitals[i].getValue());
    if (--comma) {
      Core2::copyToBuffer(',');
    }
  }
  /*
  // messages
  for (uint8_t i=0; i < Core2::messages_len; i++) {
    Core2::copyToBuffer(Core2::messages[i].value);
    if (--comma) {
      Core2::copyToBuffer(',');
    }
  }
  */
  Core2::copyToBuffer(']');
  Core2::sendBuffer();
}


void WSlave::sendBody_P(const prog_uchar *data, size_t length)
{
  Core2::copyToBuffer_P(data, length);
  Core2::sendBuffer();
  //WSlave::_client.write(data, length);
}


/**
  * copy the string starting here until the end character
  * into buffer (reduce the bufferSiez)
  *
  * @return false if end by a new line
  */
const boolean WSlave::nextHttpLine()
{
  uint8_t watchdog = MAXLINESIZE;
  char c;
  _carriageReturn:
  while ((c=WSlave::_client.read())!=CR && --watchdog && c!=-1);
  _lineFeed:
  if (watchdog && (c=WSlave::_client.read())!=LF && c!=-1) {
    goto _carriageReturn;
  }
  return watchdog != MAXLINESIZE;
}


const uint8_t WSlave::lineLength()
{
  uint8_t watchdog = MAXLINESIZE;
  char c;
  while ((c=WSlave::_client.read())!=LF && --watchdog && c!=-1);
  LOG("header length: "); LOGLN(MAXLINESIZE - watchdog);
  return MAXLINESIZE - watchdog;
}


void WSlave::sendToJson(const char type, Connector connector, const boolean comma)
{
  char pinChars[3] = { type, '0'+(connector.getPin()/10), '0'+(connector.getPin()%10) };
  Core2::copyToBuffer(pinChars, 3);
  Core2::copyToBuffer_P(json_qcolon);
  Core2::copyToBuffer_P(connector.getLabel());
  if (comma) {
    Core2::copyToBuffer_P(json_qcomma);
  }
}


void WSlave::waitClient(uint8_t& watchdog)
{
  do {
    delay(READCHAR_TIMEOUT);
  } while(!WSlave::_client.available() && --watchdog);
  /* == * /
  char c;
  LOG("> ");
  while((c=WSlave::_client.read()) && 32<=c && c<=127) {
    LOG(c);
  }
  LOGLN();
  /* == */
  WSlave::_client.flush();
}
