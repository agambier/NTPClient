/**
 * The MIT License (MIT)
 * Copyright (c) 2015 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "NTPClient.h"

#define SEVENZYYEARS		2208988800UL
#define LEAP_YEAR( Y )		( ( Y > 0 ) && !( Y % 4 ) && ( ( Y % 100 ) || !( Y % 400 ) ) )

NTPClient::NTPClient( UDP &udp, const char *poolServerName, int timeOffset, unsigned long updateInterval ) :
	m_udp( &udp ),
	m_udpSetup( false ),
	m_poolServerName( poolServerName ),
	m_port( NTP_DEFAULT_LOCAL_PORT ),
	m_timeOffset( timeOffset ),
	m_updateInterval( updateInterval ),
	m_currentEpoc(0 ),
	m_lastUpdate( 0 )
{
}

void NTPClient::begin( int port )
{
	m_port = port;
	m_udp->begin( m_port );
	m_udpSetup = true;
}

bool NTPClient::isValid(byte * ntpPacket)
{	//	Perform a few validity checks on the packet
	
	//	Check for LI=UNSYNC
	if( ( ntpPacket[ 0 ] & 0b11000000 ) == 0b11000000 )
		return false;

	//	Check for Version >= 4
	if( ( ( ntpPacket[ 0 ] & 0b00111000) >> 3 ) < 0b100 )
		return false;

	//	Check for Mode == Server
	if( ( ntpPacket[ 0 ] & 0b00000111 ) != 0b100 )
		return false;

	//	Check for valid Stratum
	if( ( ntpPacket[ 1 ] < 1 ) || ( ntpPacket[ 1 ] > 15 ) )
		return false;

	//	Check for ReferenceTimestamp != 0
	if(	( ntpPacket[ 16 ] == 0 ) && ( ntpPacket[ 17 ] == 0 ) 
	&&	( ntpPacket[ 18 ] == 0 ) && ( ntpPacket[ 19 ] == 0 )
	&&	( ntpPacket[ 20 ] == 0 ) && ( ntpPacket[ 21 ] == 0 )
	&&	( ntpPacket[ 22 ] == 0 ) && ( ntpPacket[ 22 ] == 0 ) )
		return false;

	return true;
}

bool NTPClient::forceUpdate() 
{
#ifdef DEBUG_NTPClient
	Serial.println("Update from NTP Server");
#endif

	sendNTPPacket();

	// Wait till data is there or timeout...
	byte timeout = 0;
	int cb = 0;
	do 
	{
		delay ( 10 );
		cb = m_udp->parsePacket();

		if( cb > 0 )
		{
			m_udp->read( m_packetBuffer, NTP_PACKET_SIZE );
			if( !isValid( m_packetBuffer ) )
				cb = 0;
		}

		// timeout after 1000 ms
		if( timeout > 100 ) 
			return false; 
		timeout++;
	}
	while( cb == 0 );

	m_lastUpdate = millis() - (10 * (timeout + 1)); // Account for delay in reading the time
	unsigned long highWord = word(m_packetBuffer[40], m_packetBuffer[41]);
	unsigned long lowWord = word(m_packetBuffer[42], m_packetBuffer[43]);
	// combine the four bytes (two words) into a long integer
	// this is NTP time (seconds since Jan 1 1900):
	unsigned long secsSince1900 = highWord << 16 | lowWord;
	m_currentEpoc = secsSince1900 - SEVENZYYEARS;

	return true;
}

bool NTPClient::update() 
{
	// Update after m_updateInterval or if there was no update yet.
	if( ( millis() - m_lastUpdate >= m_updateInterval )     
	||	( m_lastUpdate == 0 ) ) 
	{
		// setup the UDP client if needed
		if( !m_udpSetup ) 
			begin();                         

		return forceUpdate();
	}
	return true;
}

unsigned long NTPClient::epochTime() 
{
	return	m_timeOffset + // User offset
			m_currentEpoc + // Epoc returned by the NTP server
			( ( millis() - m_lastUpdate ) / 1000 ); // Time since last update
}


int NTPClient::hours() 
{
	return ( ( epochTime()  % 86400L ) / 3600 );
}
int NTPClient::minutes() 
{
	return ( ( epochTime() % 3600) / 60 );
}

int NTPClient::seconds() 
{
	return ( epochTime() % 60 );
}

uint16_t NTPClient::year() 
{
	uint16_t result;
	date( &result, nullptr, nullptr );
	return result;
}

uint8_t NTPClient::month() 
{
	uint8_t result;
	date( nullptr, &result, nullptr );
	return result;
}

uint8_t NTPClient::day() 
{
	uint8_t result;
	date( nullptr, nullptr, &result );
	return result;
}

void NTPClient::date( uint16_t *year, uint8_t *month, uint8_t *day, unsigned long secs ) 
{
	unsigned long rawTime = ( secs ? secs : epochTime() ) / 86400L;  // in days
	unsigned long days = 0, yearInt = 1970;
	uint8_t monthInt;
	static const uint8_t monthDays[]={ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	while( ( days += ( LEAP_YEAR( yearInt ) ? 366 : 365 ) ) <= rawTime )
	{
		yearInt++;
	}
	rawTime -= days - ( LEAP_YEAR( yearInt ) ? 366 : 365 ); // now it is days in this yearInt, starting at 0
	days = 0;
	for( monthInt = 0; monthInt < 12; monthInt++ ) 
	{
		uint8_t monthLength;
		if( monthInt==1 )  // february
			monthLength = LEAP_YEAR( yearInt ) ? 29 : 28;
		else
			monthLength = monthDays[ monthInt ];
		
		if( rawTime < monthLength ) 
			break;

		rawTime -= monthLength;
	}

	if( year )
		*year = static_cast< uint16_t >( yearInt );
	if( month )
		*month = static_cast< uint8_t >( monthInt + 1 );
	if( day )	
		*day = static_cast< uint8_t >( rawTime + 1 );
}

String NTPClient::formattedTime(unsigned long secs) 
{
	unsigned long rawTime = secs ? secs : epochTime();
	unsigned long hours = (rawTime % 86400L) / 3600;
	String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

	unsigned long minutes = (rawTime % 3600) / 60;
	String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

	unsigned long seconds = rawTime % 60;
	String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

	return hoursStr + ":" + minuteStr + ":" + secondStr;
}

// Based on https://github.com/PaulStoffregen/Time/blob/master/Time.cpp
// currently assumes UTC timezone, instead of using m_timeOffset
String NTPClient::formattedDate(unsigned long secs) 
{
	uint16_t year;
	uint8_t month, day;
	date(&year, &month, &day, secs );
	String monthStr = ( month < 10 ) ? "0" + String( month ) : String( month ); // jan is month 1  
	String dayStr = ( day < 10 ) ? "0" + String( day ) : String( day ); // day of month  
	return String( year ) + "-" + monthStr + "-" + dayStr + "T" + formattedTime( secs ) + "Z";
}

void NTPClient::end() 
{
	m_udp->stop();
	m_udpSetup = false;
}

void NTPClient::setTimeOffset(int timeOffset) 
{
	m_timeOffset     = timeOffset;
}

void NTPClient::setUpdateInterval(unsigned long updateInterval) 
{
	m_updateInterval = updateInterval;
}

void NTPClient::sendNTPPacket() 
{
	// set all bytes in the buffer to 0
	memset( m_packetBuffer, 0, NTP_PACKET_SIZE );
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	m_packetBuffer[ 0 ] = 0b11100011;   // LI, Version, Mode
	m_packetBuffer[ 1 ] = 0;     // Stratum, or type of clock
	m_packetBuffer[ 2 ] = 6;     // Polling Interval
	m_packetBuffer[ 3 ] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	m_packetBuffer[ 12 ]  = 0x49;
	m_packetBuffer[ 13 ]  = 0x4E;
	m_packetBuffer[ 14 ]  = 0x49;
	m_packetBuffer[ 15 ]  = 0x52;

	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	m_udp->beginPacket( m_poolServerName, 123 ); //NTP requests are to port 123
	m_udp->write( m_packetBuffer, NTP_PACKET_SIZE );
	m_udp->endPacket();
}

void NTPClient::setEpochTime(unsigned long secs) 
{
	m_currentEpoc = secs;
}
