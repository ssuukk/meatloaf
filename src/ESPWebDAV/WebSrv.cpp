// Meatloaf - A Commodore 1541 disk drive emulator
// https://github.com/idolpx/meatloaf
// Copyright(C) 2020 James Johnston
//
// Meatloaf is free software : you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Meatloaf is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Meatloaf. If not, see <http://www.gnu.org/licenses/>.

#include "ESPWebDAV.h"

// Sections are copied from ESP8266Webserver

// ------------------------
String ESPWebDAV::getMimeType(String path) {
// ------------------------
	if(path.endsWith(".html")) return F("text/html");
	else if(path.endsWith(".htm")) return F("text/html");
	else if(path.endsWith(".css")) return F("text/css");
	else if(path.endsWith(".txt")) return F("text/plain");
	else if(path.endsWith(".js")) return F("application/javascript");
	else if(path.endsWith(".json")) return F("application/json");
	else if(path.endsWith(".png")) return F("image/png");
	else if(path.endsWith(".gif")) return F("image/gif");
	else if(path.endsWith(".jpg")) return F("image/jpeg");
	else if(path.endsWith(".ico")) return F("image/x-icon");
	else if(path.endsWith(".svg")) return F("image/svg+xml");
	else if(path.endsWith(".ttf")) return F("application/x-font-ttf");
	else if(path.endsWith(".otf")) return F("application/x-font-opentype");
	else if(path.endsWith(".woff")) return F("application/font-woff");
	else if(path.endsWith(".woff2")) return F("application/font-woff2");
	else if(path.endsWith(".eot")) return F("application/vnd.ms-fontobject");
	else if(path.endsWith(".sfnt")) return F("application/font-sfnt");
	else if(path.endsWith(".xml")) return F("text/xml");
	else if(path.endsWith(".pdf")) return F("application/pdf");
	else if(path.endsWith(".zip")) return F("application/zip");
	else if(path.endsWith(".gz")) return F("application/x-gzip");
	else if(path.endsWith(".appcache")) return F("text/cache-manifest");

	return F("application/octet-stream");
}




// ------------------------
String ESPWebDAV::urlDecode(const String& text)	{
// ------------------------
	String decoded = "";
	char temp[] = "0x00";
	unsigned int len = text.length();
	unsigned int i = 0;
	while (i < len)	{
		char decodedChar;
		char encodedChar = text.charAt(i++);
		if ((encodedChar == '%') && (i + 1 < len))	{
			temp[2] = text.charAt(i++);
			temp[3] = text.charAt(i++);
			decodedChar = strtol(temp, NULL, 16);
		}
		else {
			if (encodedChar == '+')
				decodedChar = ' ';
			else
				decodedChar = encodedChar;  // normal ascii char
		}
		decoded += decodedChar;
	}
	return decoded;
}





// ------------------------
String ESPWebDAV::urlToUri(String url)	{
// ------------------------
	if(url.startsWith("http://"))	{
		int uriStart = url.indexOf('/', 7);
		return url.substring(uriStart);
	}
	else
		return url;
}



// ------------------------
bool ESPWebDAV::isClientWaiting() {
// ------------------------
	return server->hasClient();
}




// ------------------------
void ESPWebDAV::handleClient(String blank) {
// ------------------------
	processClient(&ESPWebDAV::handleRequest, blank);
}



// ------------------------
void ESPWebDAV::rejectClient(String rejectMessage) {
// ------------------------
	processClient(&ESPWebDAV::handleReject, rejectMessage);
}



// ------------------------
void ESPWebDAV::processClient(THandlerFunction handler, String message) {
// ------------------------
	// Check if a client has connected
	client = server->available();
	if(!client)
		return;

	// Wait until the client sends some data
	while(!client.available())
		delay(1);
	
	// reset all variables
	_chunked = false;
	_responseHeaders = String();
	_contentLength = CONTENT_LENGTH_NOT_SET;
	method = String();
	uri = String();
	contentLengthHeader = String();
	depthHeader = String();
	hostHeader = String();
	destinationHeader = String();

	// extract uri, headers etc
	if(parseRequest())
		// invoke the handler
		(this->*handler)(message);
		
	// finalize the response
	if(_chunked)
		sendContent("");

	// send all data before closing connection
	client.flush();
	// close the connection
	client.stop();
}





// ------------------------
bool ESPWebDAV::parseRequest() {
// ------------------------
	// Read the first line of HTTP request
	String req = client.readStringUntil('\r');
	client.readStringUntil('\n');
	
	// First line of HTTP request looks like "GET /path HTTP/1.1"
	// Retrieve the "/path" part by finding the spaces
	int addr_start = req.indexOf(' ');
	int addr_end = req.indexOf(' ', addr_start + 1);
	if (addr_start == -1 || addr_end == -1) {
		return false;
	}

	method = req.substring(0, addr_start);
	uri = urlDecode(req.substring(addr_start + 1, addr_end));
	// Debug_print(F("method: "); Debug_print(method); Debug_print(" url: ")); Debug_println(uri);
	
	// parse and finish all headers
	String headerName;
	String headerValue;
	
	while(1) {
		req = client.readStringUntil('\r');
		client.readStringUntil('\n');
		if(req == "") 
			// no more headers
			break;
			
		int headerDiv = req.indexOf(':');
		if (headerDiv == -1)
			break;
		
		headerName = req.substring(0, headerDiv);
		headerValue = req.substring(headerDiv + 2);
		// Debug_print(F("\t"); Debug_print(headerName); Debug_print(": ")); Debug_println(headerValue);
		
		if(headerName.equalsIgnoreCase(F("Host")))
			hostHeader = headerValue;
		else if(headerName.equalsIgnoreCase(F("Depth")))
			depthHeader = headerValue;
		else if(headerName.equalsIgnoreCase(F("Content-Length")))
			contentLengthHeader = headerValue;
		else if(headerName.equalsIgnoreCase(F("Destination")))
			destinationHeader = headerValue;
	}
	
	return true;
}




// ------------------------
void ESPWebDAV::sendHeader(const String& name, const String& value, bool first) {
// ------------------------
	String headerLine = name + ": " + value + "\r\n";

	if (first)
		_responseHeaders = headerLine + _responseHeaders;
	else
		_responseHeaders += headerLine;
}



// ------------------------
void ESPWebDAV::send(String code, const char* content_type, const String& content) {
// ------------------------
	String header;
	_prepareHeader(header, code, content_type, content.length());

	client.write(header.c_str(), header.length());
	if(content.length())
		sendContent(content);
}



// ------------------------
void ESPWebDAV::_prepareHeader(String& response, String code, const char* content_type, size_t contentLength) {
// ------------------------
	response = "HTTP/1.1 " + code + "\r\n";

	if(content_type)
		sendHeader(F("Content-Type"), content_type, true);
	
	if(_contentLength == CONTENT_LENGTH_NOT_SET)
		sendHeader(F("Content-Length"), String(contentLength));
	else if(_contentLength != CONTENT_LENGTH_UNKNOWN)
		sendHeader(F("Content-Length"), String(_contentLength));
	else if(_contentLength == CONTENT_LENGTH_UNKNOWN) {
		_chunked = true;
		sendHeader(F("Accept-Ranges"),F("none"));
		sendHeader(F("Transfer-Encoding"),F("chunked"));
	}
	sendHeader(F("Connection"), F("close"));

	response += _responseHeaders;
	response +=  F("\r\n");
}



// ------------------------
void ESPWebDAV::sendContent(const String& content) {
// ------------------------
	const char * footer = "\r\n";
	size_t size = content.length();
	
	if(_chunked) {
		char * chunkSize = (char *) malloc(11);
		if(chunkSize) {
			sprintf(chunkSize, "%x%s", size, footer);
			client.write(chunkSize, strlen(chunkSize));
			free(chunkSize);
		}
	}
	
	client.write(content.c_str(), size);
	
	if(_chunked) {
		client.write(footer, 2);
		if (size == 0) {
			_chunked = false;
		}
	}
}



// ------------------------
void ESPWebDAV::sendContent_P(PGM_P content) {
// ------------------------
	const char * footer = "\r\n";
	size_t size = strlen_P(content);
	
	if(_chunked) {
		char * chunkSize = (char *) malloc(11);
		if(chunkSize) {
			sprintf(chunkSize, "%x%s", size, footer);
			client.write(chunkSize, strlen(chunkSize));
			free(chunkSize);
		}
	}
	
	client.write_P(content, size);
	
	if(_chunked) {
		client.write(footer, 2);
		if (size == 0) {
			_chunked = false;
		}
	}
}



// ------------------------
void ESPWebDAV::setContentLength(size_t len)	{
// ------------------------
	_contentLength = len;
}


// ------------------------
size_t ESPWebDAV::readBytesWithTimeout(uint8_t *buf, size_t bufSize) {
// ------------------------
	int timeout_ms = HTTP_MAX_POST_WAIT;
	size_t numAvailable = 0;
	while(!(numAvailable = client.available()) && client.connected() && timeout_ms--) 
		delay(1);

	if(!numAvailable)
		return 0;

	return client.read(buf, bufSize);
}


// ------------------------
size_t ESPWebDAV::readBytesWithTimeout(uint8_t *buf, size_t bufSize, size_t numToRead) {
// ------------------------
	int timeout_ms = HTTP_MAX_POST_WAIT;
	size_t numAvailable = 0;
	
	while(((numAvailable = client.available()) < numToRead) && client.connected() && timeout_ms--) 
		delay(1);

	if(!numAvailable)
		return 0;

	return client.read(buf, bufSize);
}

