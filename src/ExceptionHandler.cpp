#pragma once

#include "ExceptionHandler.h"
#include <iostream>

ExceptionHandler::ExceptionHandler(const ExceptionCode& code, const std::string& message, 
                         const std::string& file, const std::string& function, long line)
: mCode( code ),
  mLine( line ),
  mFile( file ),
  mMessage( message ),
  mFunction( function )
{ }

ExceptionHandler ExceptionHandler::operator = (ExceptionHandler& right)
{
	mCode     = right.mCode;
	mLine     = right.mLine;
	mFile     = right.mFile;
	mMessage  = right.mMessage;
	mFunction = right.mFunction;

	return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////

ExceptionHandler::ExceptionCode ExceptionHandler::GetExceptionCode() const throw()
{
	return mCode;
}

std::string ExceptionHandler::GetDetailedInformation() throw()
{
	// Use a static buffer to avoid unncessary allocations on every call.
	static char sBuffer[20];

	// This is the header of our detailed exception description.
	std::string mDetailed = "Exception Has Been Caught!\r\n";
	mDetailed += std::string("---------------------------------------\r\n"
	                          "Exception Details:\r\n"
	                          "---------------------------------------\r\n");

	// Name the exception code which triggered this.
	mDetailed += "Code    : " + GetExceptionCodeStr() + "\r\n";
	
	// Use snprintf because its safer, and add the line number.
	_snprintf(sBuffer, 20, "%d", mLine);
	mDetailed += "Line    : " + std::string( sBuffer )  + "\r\n";

	// Print the name of the function from which the exception was thrown.
	mDetailed += "Function: " + mFunction + "\r\n";

	// Print the name of the file in which the offending code is found.
	mDetailed += "File    : " + mFile + "\r\n";

	// Print our detailed error message.
	mDetailed += "Message : " + mMessage;

	return mDetailed;
}

////////////////////////////////////////////////////////////////////////////////////////////////

std::string ExceptionHandler::GetExceptionCodeStr() throw()
{
	switch( mCode )
	{
	case EC_FATAL_ERROR: return "EC_FATAL_ERROR";
	case EC_GENERAL_ERROR: return "EC_GENERAL_ERROR";
	case EC_INVALID_PARAMETERS: return "EC_INVALID_PARAMETERS";
	default: return "UNKNOWN CODE";
	}
}