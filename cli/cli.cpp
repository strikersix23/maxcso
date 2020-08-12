#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#ifndef _WIN32
#include <libgen.h>
#endif
#include "winargs.h"
#include "winglob.h"
#include "../src/compress.h"
#include "../src/checksum.h"
#include "uv.h"

void show_version() {
	fprintf(stderr, "maxcso v%s\n", maxcso::VERSION);
}

void show_help(const char *arg0) {
	show_version();
	fprintf(stderr, "Usage: %s [--args] input.iso [-o output.cso]\n", arg0);
	fprintf(stderr, "\n");
	fprintf(stderr, "Multiple files may be specified.  Inputs can be iso or cso files.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   --threads=N     Specify N threads for I/O and compression\n");
	fprintf(stderr, "   --quiet         Suppress status output\n");
	fprintf(stderr, "   --crc           Log CRC32 checksums, ignore output files and methods\n");
	fprintf(stderr, "   --fast          Use only basic zlib or lz4 for fastest result\n");
	fprintf(stderr, "   --decompress    Write out to raw ISO, decompressing as needed\n");
	fprintf(stderr, "   --block=N       Specify a block size (default depends on iso size)\n");
	fprintf(stderr, "                   Many readers only support the 2048 size\n");
	fprintf(stderr, "   --format=VER    Specify cso version (options: cso1, cso2, zso, dax)\n");
	fprintf(stderr, "                   These are experimental, default is cso1\n");
	// TODO: Bring this back once it's functional.
	//fprintf(stderr, "   --smallest      Force compression of all sectors for smallest result\n");
	fprintf(stderr, "   --use-zlib      Enable trials with zlib for deflate compression\n");
	fprintf(stderr, "   --use-zopfli    Enable trials with Zopfli for deflate compression\n");
#ifndef NO_DEFLATE7Z
	fprintf(stderr, "   --use-7zdeflate Enable trials with 7-zip\'s deflate compression\n");
#endif
	fprintf(stderr, "   --use-lz4       Enable trials with lz4hc for lz4 compression\n");
	fprintf(stderr, "   --use-lz4brute  Enable bruteforce trials with lz4hc for lz4 compression\n");
	fprintf(stderr, "   --only-METHOD   Only allow a certain compression method (zlib, etc. above)\n");
	fprintf(stderr, "   --no-METHOD     Disable a certain compression method (zlib, etc. above)\n");
	fprintf(stderr, "                   The default is to use zlib and 7zdeflate only\n");
	fprintf(stderr, "   --lz4-cost=N    Allow lz4 to increase block size by N%% at most (cso2 only)\n");
	fprintf(stderr, "   --orig-cost=N   Allow uncompressed to increase block size by N%% at most\n");
	fprintf(stderr, "   --output-path=X Output to path X/, use basename for default outputs\n");
}

bool has_arg_value(int &i, char *argv[], const std::string &arg, const char *&val) {
	if (arg.compare(0, arg.npos, argv[i], arg.size()) == 0) {
		if (strlen(argv[i]) == arg.size()) {
			val = argv[++i];
			return true;
		} else if (argv[i][arg.size()] == '=') {
			val = argv[i] + arg.size() + 1;
			return true;
		}
	}

	return false;
}

bool has_arg_method(int &i, char *argv[], const std::string &arg, uint32_t &method) {
	if (arg.compare(0, arg.npos, argv[i], arg.size()) == 0) {
		const char *val = argv[i] + arg.size();
		if (strcmp(val, "zlib") == 0) {
			method = maxcso::TASKFLAG_NO_ZLIB;
			return true;
		} else if (strcmp(val, "zopfli") == 0) {
			method = maxcso::TASKFLAG_NO_ZOPFLI;
			return true;
#ifndef NO_DEFLATE7Z
		} else if (strcmp(val, "7zdeflate") == 0 || strcmp(val, "7zip") == 0) {
			method = maxcso::TASKFLAG_NO_7ZIP;
			return true;
#endif
		} else if (strcmp(val, "lz4") == 0) {
			method = maxcso::TASKFLAG_NO_LZ4_DEFAULT | maxcso::TASKFLAG_NO_LZ4_HC;
			return true;
		} else if (strcmp(val, "lz4brute") == 0) {
			method = maxcso::TASKFLAG_NO_LZ4_HC_BRUTE;
			return true;
		}
	}

	return false;
}

bool has_arg(int &i, char *argv[], const std::string &arg) {
	if (arg.compare(argv[i]) == 0) {
		return true;
	}

	return false;
}

struct Arguments {
	std::vector<std::string> inputs;
	std::vector<std::string> outputs;
	std::string output_path;
	int threads;
	uint32_t block_size;

	// Let's just use separate vars for each and figure out at the end.
	// Clearer to translate the user's logic this way, with defaults.
	uint32_t flags_fmt;
	uint32_t flags_use;
	uint32_t flags_no;
	uint32_t flags_only;
	uint32_t flags_final;

	double orig_cost_percent;
	double lz4_cost_percent;

	bool fast;
	bool smallest;
	bool quiet;
	bool crc;
	bool decompress;
};

void default_args(Arguments &args) {
	args.threads = 0;
	args.block_size = maxcso::DEFAULT_BLOCK_SIZE;

	args.flags_fmt = 0;
	args.flags_use = 0;
	args.flags_no = 0;
	args.flags_only = 0;
	args.flags_final = 0;

	args.orig_cost_percent = 0.0;
	args.lz4_cost_percent = 0.0;

	args.fast = false;
	args.smallest = false;
	args.quiet = false;
	args.crc = false;
	args.decompress = false;
}

void wildcard_to_inputs(const char *arg, std::vector<std::string> &files) {
#ifdef _WIN32
	winargs_get_wildcard(arg, files);
#else
	// TODO: Use glob.
	files.push_back(arg);
#endif
}

int parse_args(Arguments &args, int argc, char *argv[]) {
	const char *val = nullptr;
	uint32_t method = 0;
	int i;
	for (i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (has_arg(i, argv, "--help") || has_arg(i, argv, "-h")) {
				show_help(argv[0]);
				return 1;
			} else if (has_arg(i, argv, "--version") || has_arg(i, argv, "-v")) {
				show_version();
				return 1;
			} else if (has_arg_value(i, argv, "--block", val)) {
				args.block_size = atoi(val);
			} else if (has_arg_value(i, argv, "--threads", val)) {
				args.threads = atoi(val);
			} else if (has_arg_value(i, argv, "--orig-cost", val)) {
				args.orig_cost_percent = atof(val);
			} else if (has_arg_value(i, argv, "--lz4-cost", val)) {
				args.lz4_cost_percent = atof(val);
			} else if (has_arg_value(i, argv, "--format", val)) {
				if (strcmp(val, "cso1") == 0) {
					args.flags_fmt = 0;
				} else if (strcmp(val, "cso2") == 0) {
					args.flags_fmt = maxcso::TASKFLAG_FMT_CSO_2;
				} else if (strcmp(val, "zso") == 0) {
					args.flags_fmt = maxcso::TASKFLAG_FMT_ZSO;
				} else if (strcmp(val, "dax") == 0) {
					args.flags_fmt = maxcso::TASKFLAG_FMT_DAX;
				} else {
					show_help(argv[0]);
					fprintf(stderr, "\nERROR: Unknown format %s, expecting cso1, cso2, or zso.\n", val);
					return 1;
				}
			} else if (has_arg(i, argv, "--crc")) {
				args.crc = true;
			} else if (has_arg(i, argv, "--quiet")) {
				args.quiet = true;
			} else if (has_arg(i, argv, "--fast")) {
				args.fast = true;
			} else if (has_arg(i, argv, "--smallest")) {
				args.smallest = true;
			} else if (has_arg(i, argv, "--decompress")) {
				args.decompress = true;
			} else if (has_arg_method(i, argv, "--use-", method)) {
				args.flags_use |= method;
			} else if (has_arg_method(i, argv, "--no-", method)) {
				args.flags_no |= method;
			} else if (has_arg_method(i, argv, "--only-", method)) {
				args.flags_only |= method;
			} else if (has_arg_value(i, argv, "--output-path", val)) {
				args.output_path = val;
				// Don't treat this as just a prefix, it's confusing with the basename behavior.
				if (strlen(val) >= 1 && val[strlen(val) - 1] != '/') {
					args.output_path += "/";
				}
			} else if (has_arg_value(i, argv, "--out", val) || has_arg_value(i, argv, "-o", val)) {
				args.outputs.push_back(args.output_path + val);
			} else if (has_arg(i, argv, "--")) {
				break;
			} else {
				show_help(argv[0]);
				fprintf(stderr, "\nERROR: Unknown argument: %s\n", argv[i]);
				return 1;
			}
		} else {
			wildcard_to_inputs(argv[i], args.inputs);
		}
	}

	for (; i < argc; ++i) {
		// If we're here, it means we hit a "--".  The rest are inputs, not args.
		wildcard_to_inputs(argv[i], args.inputs);
	}

	return 0;
}

static std::string get_basename(std::string &filename) {
#ifdef _WIN32
	char name[MAX_PATH];
	char ext[MAX_PATH];
	// We want the ext because it might be "foo.bar.baz".
	if (_splitpath_s(filename.c_str(), nullptr, 0, nullptr, 0, name, MAX_PATH, ext, MAX_PATH) == 0) {
		return std::string(name) + ext;
	}
	return filename;
#else
	filename.resize(filename.size() + 1, '\0');
	return basename(&filename[0]);
#endif
}

int validate_args(const char *arg0, Arguments &args) {
	if (args.threads == 0) {
		uv_cpu_info_t *cpus;
		uv_cpu_info(&cpus, &args.threads);
		uv_free_cpu_info(cpus, args.threads);
	}

	if (args.inputs.size() < args.outputs.size()) {
		show_help(arg0);
		fprintf(stderr, "\nERROR: Too many output files.\n");
		return 1;
	}

	if (args.crc) {
		if (args.outputs.size()) {
			show_help(arg0);
			fprintf(stderr, "\nERROR: Output files not used with --crc.\n");
			return 1;
		}
	} else {
		std::string outputExt = ".cso";
		if (args.flags_fmt & maxcso::TASKFLAG_FMT_DAX) {
			outputExt = ".dax";
		} else if (args.flags_fmt & maxcso::TASKFLAG_FMT_ZSO) {
			outputExt = ".zso";
		}

		// Automatically write to .cso files if not specified.
		for (size_t i = args.outputs.size(); i < args.inputs.size(); ++i) {
			if (args.inputs[i].size() <= 4) {
				continue;
			}

			std::string ext = args.inputs[i].substr(args.inputs[i].size() - 4);
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			const bool inputRawExt = ext == ".iso";
			const bool inputCompressedExt = ext == ".cso" || ext == ".zso" || ext == ".dax";

			std::string without_ext = args.inputs[i].substr(0, args.inputs[i].size() - 4);
			if (!args.output_path.empty()) {
				without_ext = args.output_path + get_basename(without_ext);
			}

			// Automatically switch extensions for convenience.
			if (!args.decompress && (inputRawExt || inputCompressedExt) && ext != outputExt) {
				args.outputs.push_back(without_ext + outputExt);
			} else if (args.decompress && inputCompressedExt) {
				args.outputs.push_back(without_ext + ".iso");
			}
		}

		if (args.inputs.size() != args.outputs.size()) {
			show_help(arg0);
			fprintf(stderr, "\nERROR: Too few output files.\n");
			return 1;
		}
	}

	if (args.inputs.empty()) {
		show_help(arg0);
		fprintf(stderr, "\nERROR: No input files.\n");
		return 1;
	}

	// Cleanup flags: defaults first, by format.
	if (args.flags_fmt & maxcso::TASKFLAG_FMT_CSO_2) {
		args.flags_final = maxcso::TASKFLAG_NO_ZOPFLI | maxcso::TASKFLAG_NO_LZ4_HC_BRUTE;
	} else if (args.flags_fmt & maxcso::TASKFLAG_FMT_ZSO) {
		args.flags_final = maxcso::TASKFLAG_NO_ZLIB | maxcso::TASKFLAG_NO_7ZIP | maxcso::TASKFLAG_NO_ZOPFLI | maxcso::TASKFLAG_NO_LZ4_HC_BRUTE;
	} else {
		// CSO v1 or DAX, just disable lz4.
		args.flags_final = maxcso::TASKFLAG_NO_ZOPFLI | maxcso::TASKFLAG_NO_LZ4;
	}

	// Kill any of the NO flags for the --use-METHOD args.
	args.flags_final &= ~args.flags_use;
	// Then insert NO flags for --no-METHOD args.
	args.flags_final |= args.flags_no;

	// Lastly, if --only-METHOD was used, set all NOs and kill those NOs only.
	if (args.flags_only != 0) {
		args.flags_final |= maxcso::TASKFLAG_NO_ALL;
		args.flags_final &= ~args.flags_only;
	}

	if (args.fast) {
		args.flags_final |= maxcso::TASKFLAG_NO_ZLIB_BRUTE | maxcso::TASKFLAG_NO_ZOPFLI | maxcso::TASKFLAG_NO_7ZIP | maxcso::TASKFLAG_NO_LZ4_HC_BRUTE | maxcso::TASKFLAG_NO_LZ4_HC;
	}
	if (args.smallest) {
		args.flags_final |= maxcso::TASKFLAG_FORCE_ALL;
	}
	if (args.decompress) {
		args.flags_final |= maxcso::TASKFLAG_DECOMPRESS;
	}
	args.flags_final |= args.flags_fmt;

	if (args.flags_fmt & maxcso::TASKFLAG_FMT_DAX) {
		// DAX has a fixed block size.
		if (args.block_size != maxcso::DEFAULT_BLOCK_SIZE) {
			show_help(arg0);
			fprintf(stderr, "\nERROR: Block size must be default for DAX.\n");
			return 1;
		}

		// Currently, compression will fail if no DEFLATE format is enabled for DAX.
		uint32_t deflateFlags = maxcso::TASKFLAG_NO_ZLIB | maxcso::TASKFLAG_NO_ZLIB_DEFAULT | maxcso::TASKFLAG_NO_ZLIB_BRUTE | maxcso::TASKFLAG_NO_ZOPFLI | maxcso::TASKFLAG_NO_7ZIP;
		if ((args.flags_final & deflateFlags) == deflateFlags) {
			show_help(arg0);
			fprintf(stderr, "\nERROR: DAX must use some kind of DEFLATE.\n");
			return 1;
		}
	}

	return 0;
}

#ifdef _WIN32
int setenv(const char *name, const char *value, int) {
	return _putenv_s(name, value);
}
#endif

inline uv_buf_t uv_buf_init(const char *str) {
	return uv_buf_init(const_cast<char *>(str), static_cast<unsigned int>(strlen(str)));
}

inline uv_buf_t uv_buf_init(const std::string &str) {
	return uv_buf_init(const_cast<char *>(str.c_str()), static_cast<unsigned int>(str.size()));
}

const std::string ANSI_RESET_LINE = "\033[2K\033[0G";

void update_threadpool(const Arguments &args) {
	char threadpool_size[32];
	sprintf(threadpool_size, "%d", args.threads);
	setenv("UV_THREADPOOL_SIZE", threadpool_size, 1);
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
	argv = winargs_get_utf8(argc);
#endif

	Arguments args;
	default_args(args);
	int result = parse_args(args, argc, argv);
	if (result != 0) {
		return result;
	}
	result = validate_args(argv[0], args);
	if (result != 0) {
		return result;
	}

	update_threadpool(args);

	uv_loop_t loop;
	uv_tty_t tty;
	uv_loop_init(&loop);
	uv_tty_init(&loop, &tty, 2, 0);
	uv_tty_set_mode(&tty, 0);
	bool formatting = uv_guess_handle(2) == UV_TTY && !args.quiet;

	int64_t next = uv_hrtime();
	int64_t lastPos = 0;
	// 50ms
	static const int64_t interval_ns = 50000000LL;
	static const double interval_to_s = 1000.0 / 50.0;

	std::string statusInfo;
	uv_write_t write_req;
	uv_buf_t bufs[2];

	maxcso::ProgressCallback progress = [&] (const maxcso::Task *task, maxcso::TaskStatus status, int64_t pos, int64_t total, int64_t written) {
		if (!formatting) {
			return;
		}

		statusInfo.clear();

		if (status == maxcso::TASK_INPROGRESS) {
			int64_t now = uv_hrtime();
			if (now >= next) {
				double percent = total == 0 ? 0.0 : (pos * 100.0) / total;
				double ratio = pos == 0 ? 0.0 : (written * 100.0) / pos;
				double speed = 0.0;
				int64_t diff = pos - lastPos;
				if (diff > 0) {
					speed = (diff / 1024.0 / 1024.0) * interval_to_s;
				}

				char temp[128];
				sprintf(temp, "%3.0f%%, ratio=%3.0f%%, speed=%5.2f MB/s", percent, ratio, speed);
				statusInfo = temp;

				next = now + interval_ns;
				lastPos = pos;
			}
		} else if (status == maxcso::TASK_SUCCESS) {
			statusInfo = "Complete\n";
		} else {
			// This shouldn't happen.
			statusInfo = "Something went wrong.\n";
		}

		if (statusInfo.empty()) {
			return;
		}

		unsigned int nbufs = 0;
		if (formatting) {
			bufs[nbufs++] = uv_buf_init(ANSI_RESET_LINE);
		}

		if (task->input.size() > 38) {
			statusInfo = "..." + task->input.substr(task->input.size() - 35) + ": " + statusInfo;
		} else {
			statusInfo = task->input + ": " + statusInfo;
		}

		bufs[nbufs++] = uv_buf_init(statusInfo);
		uv_write(&write_req, reinterpret_cast<uv_stream_t *>(&tty), bufs, nbufs, nullptr);
	};
	maxcso::ErrorCallback error = [&] (const maxcso::Task *task, maxcso::TaskStatus status, const char *reason) {
		// Change the result to indicate failure.
		if (status != maxcso::TASK_SUCCESS) {
			result = 1;
		}

		if (args.quiet) {
			return;
		}

		const std::string prefix = status == maxcso::TASK_SUCCESS ? "" : "Error while processing ";
		statusInfo = (formatting ? ANSI_RESET_LINE : "") + prefix + task->input + ": " + reason + "\n";
		bufs[0] = uv_buf_init(statusInfo);
		uv_write(&write_req, reinterpret_cast<uv_stream_t *>(&tty), bufs, 1, nullptr);
	};

	std::vector<maxcso::Task> tasks;
	for (size_t i = 0; i < args.inputs.size(); ++i) {
		maxcso::Task task;
		task.input = args.inputs[i];
		if (!args.crc) {
			task.output = args.outputs[i];
		}
		task.progress = progress;
		task.error = error;
		task.block_size = args.block_size;
		task.flags = args.flags_final;
		task.orig_max_cost_percent = args.orig_cost_percent;
		task.lz4_max_cost_percent = args.lz4_cost_percent;
		tasks.push_back(std::move(task));
	}

	if (args.crc) {
		maxcso::Checksum(tasks);
	} else {
		maxcso::Compress(tasks);
	}

	uv_tty_reset_mode();
	uv_loop_close(&loop);

	return result;
}
