#include "distreboot_config.h"
#include <x/options.H>

#include "distreboot.H"
#include "distreboot.opts.H"

int main(int argc, char **argv)
{
	distreboot_options opts;

	auto parser=opts.parse(argc, argv);

	if (!opts.start->value && !opts.stop->value)
	{
		parser->usage(std::wcout);
		exit(1);
	}

	distreboot::base::args args=distreboot::base::args
		::create(opts.start->value,
			 opts.stop->value,
			 opts.node->value);

	auto ret=x::singletonapp
		::managed([] { return distreboot::create(); }, args);

	if (ret.first->message.size() > 0)
		std::cout << ret.first->message << std::endl;
	exit(ret.first->exitcode);
}
