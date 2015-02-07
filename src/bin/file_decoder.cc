#include <cassert>
#include <cstdio>
#include <cstring>
#include "arg_parser.h"
#include "bin_helpers.h"
#include "factory/converter_factory.h"
#include "file_io.h"
#include "formats/converter.h"
#include "fs.h"
#include "logger.h"
#include "output_files.h"

namespace
{
    typedef struct
    {
        std::string input_path;
        std::string target_path;
    } PathInfo;

    typedef struct
    {
        const char *format;
        const char *output_dir;
        std::vector<PathInfo*> input_paths;
    } Options;

    typedef struct
    {
        Options *options;
        ArgParser &arg_parser;
        ConverterFactory &conv_factory;
        const PathInfo *path_info;
    } ReadContext;

    void add_output_folder_option(ArgParser &arg_parser, Options *options)
    {
        arg_parser.add_help(
            "-o, --out=FOLDER",
            "Where to put the output files.");

        if (arg_parser.has_switch("-o"))
            options->output_dir = arg_parser.get_switch("-o").c_str();
        if (arg_parser.has_switch("--out"))
            options->output_dir = arg_parser.get_switch("--out").c_str();
    }

    bool add_input_paths_option(const ArgParser &arg_parser, Options *options)
    {
        const std::vector<std::string> stray = arg_parser.get_stray();
        for (size_t i = 1; i < stray.size(); i ++)
        {
            std::string path = stray[i];
            if (is_dir(path))
            {
                std::vector<std::string> sub_paths = get_files_recursive(path);
                for (size_t j = 0; j < sub_paths.size(); j ++)
                {
                    PathInfo *pi = new PathInfo;
                    pi->input_path = sub_paths[j];
                    pi->target_path = pi->input_path.substr(path.length() + 1);
                    options->input_paths.push_back(pi);
                }
            }
            else
            {
                PathInfo *pi = new PathInfo;
                pi->input_path = path;
                pi->target_path = basename(path);
                options->input_paths.push_back(pi);
            }
        }

        if (options->input_paths.size() < 1)
        {
            log_error("Required more arguments.");
            return false;
        }

        return true;
    }

    void add_format_option(ArgParser &arg_parser, Options *options)
    {
        arg_parser.add_help(
            "-f, --fmt=FORMAT",
            "Selects the file format.");

        if (arg_parser.has_switch("-f"))
            options->format = arg_parser.get_switch("-f").c_str();
        if (arg_parser.has_switch("--fmt"))
            options->format = arg_parser.get_switch("--fmt").c_str();
    }

    void print_help(
        const char *path_to_self,
        ArgParser &arg_parser,
        Options *options,
        Converter *converter)
    {
        assert(path_to_self != nullptr);
        assert(options != nullptr);

        printf(
            "Usage: %s [options] [file_options] input_path [input_path...]",
            path_to_self);

        puts("");
        puts("[options] can be:");
        puts("");
        arg_parser.print_help();
        arg_parser.clear_help();
        puts("");

        if (converter != nullptr)
        {
            converter->add_cli_help(arg_parser);
            printf("[file_options] specific to %s:\n", options->format);
            puts("");
            arg_parser.print_help();
            return;
        }
        puts("[file_options] depend on chosen format and are required at "
            "runtime.");
        puts("See --help --fmt=FORMAT to get detailed help for given "
            "converter.");
    }

    void decode(
        Converter &converter,
        ArgParser &arg_parser,
        VirtualFile &file)
    {
        converter.parse_cli_options(arg_parser);
        converter.decode(file);
    }

    void guess_converter_and_decode(
        Options *options,
        ArgParser &arg_parser,
        ConverterFactory &conv_factory,
        VirtualFile &file)
    {
        assert(options != nullptr);

        bool result = false;
        if (options->format == nullptr)
        {
            bool recognized = false;
            for (auto& format : conv_factory.get_formats())
            {
                std::unique_ptr<Converter> converter(
                    conv_factory.create_converter(format));

                try
                {
                    log_info("Trying %s...", format.c_str());
                    decode(*converter, arg_parser, file);
                    log_info("Success - %s decoding finished", format.c_str());
                    recognized = true;
                    break;
                }
                catch (std::exception &e)
                {
                    log_error("%s", e.what());
                    log_info(
                        "%s didn\'t work, trying next format...",
                        format.c_str());
                    continue;
                }
            }

            if (!recognized)
                log_error("Nothing left to try. File not recognized.");
        }
        else
        {
            std::unique_ptr<Converter> converter(
                conv_factory.create_converter(options->format));

            try
            {
                decode(*converter, arg_parser, file);
                log_info("Success - %s decoding finished", options->format);
            }
            catch (std::exception &e)
            {
                log_error("%s", e.what());
                log_info("Failure - %s decoding finished", options->format);
            }
        }
    }

    void set_file_path(VirtualFile &file, std::string input_path)
    {
        file.name = input_path + "~";
    }

    std::unique_ptr<VirtualFile> read_and_decode(void *_context)
    {
        ReadContext *context = (ReadContext*)_context;

        FileIO io(context->path_info->input_path.c_str(), "rb");
        std::unique_ptr<VirtualFile> file;
        file->io.write_from_io(io, io.size());
        file->io.seek(0);

        set_file_path(
            *file,
            context->options->output_dir == nullptr
                ? context->path_info->input_path
                : context->path_info->target_path);

        guess_converter_and_decode(
            context->options,
            context->arg_parser,
            context->conv_factory,
            *file);

        return file;
    }

    bool run(
        Options *options,
        ArgParser &arg_parser,
        ConverterFactory &conv_factory)
    {
        size_t i;
        assert(options != nullptr);

        OutputFilesHdd output_files(options->output_dir);
        ReadContext context =
        {
            options,
            arg_parser,
            conv_factory,
            nullptr,
        };

        bool result = true;
        for (i = 0; i < options->input_paths.size(); i ++)
        {
            context.path_info = options->input_paths[i];
            result &= output_files.save(&read_and_decode, &context);
        }
        return result;
    }
}

int main(int argc, const char **argv)
{
    int exit_code = 0;
    Options options;
    options.format = nullptr;
    options.output_dir = nullptr;

    ConverterFactory conv_factory;
    ArgParser arg_parser;
    arg_parser.parse(argc, argv);

    add_output_folder_option(arg_parser, &options);
    add_format_option(arg_parser, &options);
    add_quiet_option(arg_parser);
    add_help_option(arg_parser);

    if (should_show_help(arg_parser))
    {
        Converter *converter = options.format != nullptr
            ? conv_factory.create_converter(options.format)
            : nullptr;
        print_help(argv[0], arg_parser, &options, converter);
        if (converter != nullptr)
            delete converter;
    }
    else
    {
        if (!add_input_paths_option(arg_parser, &options))
            exit_code = 1;
        else if (!run(&options, arg_parser, conv_factory))
            exit_code = 1;
    }

    for (auto& pi : options.input_paths)
        delete pi;
    return exit_code;
}
