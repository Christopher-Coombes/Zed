#include "../utils/utils.h"
#include "../utils/bytecode.h"

namespace executor {
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Constants?

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Executor Settings

	static constexpr int FLAG_CHECK_MEM = Flags::FLAG_FIRST_FREE;

	// Holds settings info about the execution process
	struct ExecutorSettings {
		Flags flags;
		int stackSize;

		ExecutorSettings() noexcept;
	};

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Executor Exceptions

	// An exception from the executor
	class ExecutorException : public std::runtime_error {
	public:
		enum class ErrorType {
			UNKNOWN_OPCODE,
			DIVIDE_BY_ZERO,
			BAD_ALLOC
		};

		static constexpr const char* const errorTypeStrings[] = {
			"Unknown opcode",
			"Division (or modulo) by zero",
			"Dynamic memory allocation error"
		};

	private:
		const ErrorType eType;
		const int loc;

	public:
		ExecutorException(const ErrorType eType, const int loc);
		ExecutorException(const ErrorType eType, const int loc, const char* const extra);
		//ExecutorException(const ErrorType eType, const int loc, const std::string& extra);

		int getLoc() const noexcept;
	};

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// The Stack

	// Acts as the stack memory for the program execution
	class Stack {
	private:
		const std::unique_ptr<char[]> owner;

	public:
		explicit Stack(const int size);

		char* begin() const noexcept;
	};

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Executor Functions

	// Execute a .eze file with given settings
	int exec(const char* const path, const ExecutorSettings& settings);
	int exec_(std::iostream& file, const ExecutorSettings& settings, std::ostream& outstream, std::istream& instream);
}