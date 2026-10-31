class ExceptionHandler
{
public:
	///////////////////////////////////////////////////////////////////////////
	// Enumerations

	/// An enumeration with individual exception categorizations.
	enum ExceptionCode
	{
		EC_FATAL_ERROR,
		EC_GENERAL_ERROR,
		EC_INVALID_PARAMETERS,
		EC_ILLEGAL_ADDRESS,
		EC_ILLEGAL_OPCODE,
		EC_ILLEGAL_PACKET
	};
	
	///////////////////////////////////////////////////////////////////////////
	// Constructors
	
	/** The basic constructor.
	  */
	ExceptionHandler(const ExceptionCode& code, const std::string& message, const std::string& file, 
				const std::string& function, long line);
	/** The assignment operator.
	  */
	ExceptionHandler operator = (ExceptionHandler& right);

	///////////////////////////////////////////////////////////////////////////
	// Exception Information Functions

	/** Returns the exception code for this message.
	  */
	ExceptionCode GetExceptionCode() const throw();
	/** Returns detailed information about the error.
	  */
	std::string GetDetailedInformation() throw();
protected:
	///////////////////////////////////////////////////////////////////////////
	// Exception Code Function

	/** Returns the name of the exception code for this exception.
	  */
	std::string GetExceptionCodeStr() throw();

	///////////////////////////////////////////////////////////////////////////
	// Variables

	long          mLine;
	ExceptionCode mCode;
	std::string   mFile;
	std::string   mMessage;
	std::string   mFunction;
};