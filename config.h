#ifndef ECONFIG_H_
#define ECONFIG_H_


#define EMAIL         "<YourAddress@gmail.com>"
#define ML_SUBJECT    "[Wslave#" DEVICE_ID "] "

// ETH conf
#define USE_BONJOUR     0
#define USE_DHCP        0
#define IP              10,240,170, DEVICE_NUMBER
#define SUBNET          255,255,255,0
#define GATEWAY         0,0,0,0
#define DNS             0,0,0,0
// ascii code for "@lan#" + HEX 12
#define MAC             0x40,0x6C,0x61,0x6E,0x23, DEVICE_NUMBER
#define PORT            80
#define ETH_BLPIN       53 /* power of W5100 chip */
#define HTTP_AUTH64     "YXJkbWluOkBsYW4jMTI=" // base64("ardmin:@lan#12"); /!\ chars must be parsed by reading buffer
#define SMTP_IP         173,194,67,26 // ping gmail-smtp-in.l.google.com
#define SMTP_PORT       25
// 0.fr.pool.ntp.org
#define NTP_SERVER      88, 191, 80, 53 // or use GATEWAY if not specified
#define NTP_PORT        123
#define NTP_LOCALPORT   3669 /* OR "ODE 8400" */
#define NTP_PACKET_SIZE 48
#define TZ_OFFSET       (+1) /* no DST by default (case of 12/31/y and 01/01/y+1)  */
#define TZ_DST          1


#endif
