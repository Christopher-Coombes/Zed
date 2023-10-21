#include "main.h"

#define CHECK_ARGS(str) if (argc - i < 1) {cout << IO_ERR "Not enough arguments for " str IO_NORM IO_END;return 1;}
int main(int argc, const char* args[]) {
	using namespace std;

	cout << IO_NORM;

	// cout << args[0] << "\n";

	assembler::AssemblerSettings asmSettings;
	disassembler::DisassemblerSettings disasmSettings;
	executor::ExecutorSettings execSettings;

	int out = 0;

	for (int clarg, i = 1; i < argc; i++) {
		clarg = lookupString(args[i], clargStrings, clargCount);
		if (clarg == -1) {
			cout << IO_WARN "Unknown command line argument: " << args[i] << " " IO_NORM "\n";
			break;
		}
		switch (static_cast<CLArg>(clarg)) {
			case CLArg::DEBUG:
				// TODO: Set debug flags
				asmSettings.flags.setFlags(Flags::FLAG_DEBUG);
				execSettings.flags.setFlags(Flags::FLAG_DEBUG);
				disasmSettings.flags.setFlags(Flags::FLAG_DEBUG);
				break;
			case CLArg::NODEBUG:
				// TODO: Unset debug flags 
				asmSettings.flags.unsetFlags(Flags::FLAG_DEBUG);
				execSettings.flags.setFlags(Flags::FLAG_DEBUG);
				disasmSettings.flags.setFlags(Flags::FLAG_DEBUG);
				break;
			case CLArg::ASSEMBLE:
				i += 2;
				CHECK_ARGS("assembler");
				out = assembler::assemble(args[i - 1], args[i], asmSettings);
				break;
			case CLArg::DISASSEMBLE:
				// TODO: Disassembler
				i += 2;
				CHECK_ARGS("disassembler");
				out = disassembler::disassemble(args[i - 1], args[i], disasmSettings);
				break;
			case CLArg::EXECUTE:
				i++;
				CHECK_ARGS("executor");
				out = executor::exec(args[i], execSettings);
				break;
			case CLArg::COMPILE:
				// TODO: Compiler
				break;
		}
		if (out) {
			return 1;
		}
	}

	return 0;
}