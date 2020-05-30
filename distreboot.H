/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef distreboot_H
#define distreboot_H

#include <x/ref.H>
#include <x/ptr.H>
#include <x/fd.H>
#include <x/threadmsgdispatcher.H>
#include <x/managedsingletonapp.H>
#include <x/logger.H>
#include <x/property_value.H>
#include <x/hms.H>

#include <stasher/client.H>
#include <stasher/current.H>
#include <stasher/heartbeat.H>

class distrebootObj;
class distrebootBase;
class distreboot_options;

typedef x::ref<distrebootObj, distrebootBase> distreboot;

typedef x::ptr<distrebootObj, distrebootBase> distrebootptr;

// The main class. It's an application singleton.

class distrebootObj : public x::threadmsgdispatcherObj {

protected:
	LOG_CLASS_SCOPE;

protected:
	stasher::client *client; // On the executing thread's stack

	class serverStatusCallbackObj;

	// The rebootlist object in the repository

	class rebootListObj : virtual public x::obj {

	public:

		// This object's uuid in the repository

		x::uuid uuid;

		// List of nodes that need to be rebooted. The first one in the
		// list is the one that's currently rebooting.
		std::list<std::string> list;

		// Default constructor
		rebootListObj();

		// Construct from retrieved object file descriptor
		rebootListObj(const x::uuid &uuidArg, const x::fd &fdArg);

		// Corrupted object, construct an empty reboot list
		rebootListObj(const x::uuid &uuidArg);

		// Create a string to go into the repository
		std::string to_string() const;

		~rebootListObj();
	};

	// Some convenience typedefs
	typedef x::ref<rebootListObj> rebootlist;
	typedef x::ptr<rebootListObj> rebootlistptr;

	// Current rebootlist object. Use a versionedptr, so that it
	// gets updated by a versioned put.

	typedef stasher::currentObj<rebootlistptr,
				    stasher::versionedptr<rebootlistptr>
				    > currentRebootListObj;
	typedef x::ref<currentRebootListObj> currentRebootList;

	// The state of the repository
	stasher::req_stat_t	connection_status;
	stasher::clusterstate	connection_state;
	stasher::userhelo	connection_info;

	std::string nodename;
	bool connection_state_received;
	bool connection_info_received;
	bool heartbeat_received;
	bool rebootlist_received;

	bool rebootlist_check_done;
	// Initial check, after starting, to remove my node from the reboot
	// list, if it's at the top of the list.

	typedef stasher::heartbeat<std::string, void> heartbeat_t;
	typedef stasher::heartbeatptr<std::string, void> heartbeatptr_t;

	heartbeatptr_t *heartbeatptr_instance;
	// On the executing thread's stack

	stasher::manager *managerp; // On the executing thread's stack
	stasher::heartbeatptr<std::string, void>::obj_type *heartbeatp;
	// On the executing thread's stack, when the heartbeat object is
	// created.

	currentRebootList *rebootlist_instance; // On the executing thread's stack

	void do_process_rebootlist();
	virtual void do_just_rebooted();

	std::pair<rebootlistptr, std::string> create_rebootlist();
public:
	// The name of the heartbeat object in the repository.

	static const char heartbeat_object[];

	// The name of the reboot list object in the repository.

	static const char rebootlist_object[];

	// Arguments to the singleton's invocation

	class argsObj : virtual public x::obj {

	public:
		bool start; // --start command line option
		bool stop;  // --stop
		bool reboot; // --reboot
		bool cancel; // --cancel
		bool dry_run; // --dry-run
		std::string drop; // --drop
		std::string node;  // --repo

		std::string forcenodename; // Used by regression tests

		argsObj() : start(false), stop(false),
			    reboot(false), cancel(false), dry_run(false) {}
		argsObj(const distreboot_options &opts);

		~argsObj()
		{
		}

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
			iter(ptr->start);
			iter(ptr->stop);
			iter(ptr->reboot);
			iter(ptr->cancel);
			iter(ptr->dry_run);
			iter(ptr->node);
			iter(ptr->drop);
		}
	};

	typedef x::ref<argsObj> args;
	typedef x::ptr<argsObj> argsptr;

	// Return value from the singleton.

	class retObj : virtual public x::obj {

	public:
		retObj() : exitcode(0) {}
		~retObj() {}

		std::string message;
		int exitcode;

		retObj(const std::string &messageArg,
		       int exitcodeArg) : message(messageArg),
					  exitcode(exitcodeArg)
		{
		}

		template<typename ptr_type, typename iter_type>
		static void serialize(ptr_type ptr, iter_type &iter)
		{
			iter(ptr->message);
			iter(ptr->exitcode);
		}
	};

	typedef x::ref<retObj> ret;

	distrebootObj();
	~distrebootObj();

	// The singleton thread.
	ret run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		uid_t uid, argsptr &args);

	// Do the actual reboot of this node
	// Punt it to a subclass. The default implementation just stop()s.

	virtual void do_reboot();

#include "distreboot.msgs.H"

protected:

	//! Process a serverstate() message.

	virtual void do_dispatch_serverstate(const stasher::clusterstate &state);

	//! Process a serverinfo() message.

	virtual void do_dispatch_serverinfo(const stasher::userhelo &serverinfo);
};

// distreboot::base provides some convenience typedefs.

class distrebootBase : public x::ptrref_base {

public:

	typedef distrebootObj::args args;

	typedef distrebootObj::argsptr argsptr;

	typedef distrebootObj::ret ret;
};

extern x::property::value<std::string> reboot_cmd;

#endif
