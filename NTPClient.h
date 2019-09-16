#ifndef __NTPClient_H__
#define __NTPClient_H__

#include "Arduino.h"
#include <Udp.h>

#define NTP_SERVER				"pool.ntp.org"
#define NTP_PACKET_SIZE			48
#define NTP_DEFAULT_LOCAL_PORT	1337

class NTPClient 
{
	private:
		UDP* m_udp;
		bool m_udpSetup = false;
		const char* m_poolServerName;
		int m_port = NTP_DEFAULT_LOCAL_PORT;
		int m_timeOffset;
		unsigned long m_updateInterval;		// In ms
		unsigned long m_currentEpoc;	// In s
		unsigned long m_lastUpdate;		// In ms
		bool m_ready;
		byte m_packetBuffer[ NTP_PACKET_SIZE ];

		void sendNTPPacket();
		bool isValid( byte * ntpPacket );

  public:
		NTPClient( UDP &udp, const char *poolServerName = NTP_SERVER, int timeOffset = 0, unsigned long updateInterval = 60000 );

		/**
		 * Starts the underlying UDP client with the specified local port
		 */
		void begin( int port = NTP_DEFAULT_LOCAL_PORT );

		/**
		 * Stops the underlying UDP client
		 */
		void end();

		/**
		 * This should be called in the main loop of your application. By default an update from the NTP Server is only
		 * made every 60 seconds. This can be configured in the NTPClient constructor.
		 *
		 * @return true on success, false on failure
		 */
		bool update();

		/**
		 * This will force the update from the NTP Server.
		 *
		 * @return true on success, false on failure
		 */
		bool forceUpdate();

		inline bool isReady() const;

		uint16_t year();
		uint8_t month();
		uint8_t day();
		int hours();
		int minutes();
		int seconds();

		/**
		 * Changes the time offset. Useful for changing timezones dynamically
		 */
		void setTimeOffset( int timeOffset );

		/**
		 * Set the update interval to another frequency. E.g. useful when the
		 * timeOffset should not be set in the constructor
		 */
		void setUpdateInterval( unsigned long updateInterval );

		/**
		* @return secs argument (or 0 for current time) formatted like `hh:mm:ss`
		*/
		String formattedTime( unsigned long secs = 0 );

		/**
		 * @return time in seconds since Jan. 1, 1970
		 */
		unsigned long epochTime();

		/**
		* @return secs argument (or 0 for current date) formatted to ISO 8601
		* like `2004-02-12T15:19:21+00:00`
		*/
		String formattedDate( unsigned long secs = 0 );

		void date( uint16_t *year, uint8_t *month, uint8_t *day, unsigned long secs = 0 );


		/**
		* Replace the NTP-fetched time with seconds since Jan. 1, 1970
		*/
		void setEpochTime( unsigned long secs );
};

//	----- inline functions -----
bool NTPClient::isReady() const {
	return m_ready;
}


#endif