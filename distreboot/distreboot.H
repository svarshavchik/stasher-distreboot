#ifndef distreboot_H
#define distreboot_H

#include <x/ref.H>
#include <x/ptr.H>
#include <x/fd.H>
#include <x/eventqueuemsgdispatcher.H>
#include <x/managedsingletonapp.H>
#include <x/logger.H>
#include <x/property_value.H>
#include <x/hms.H>

#include <stasher/client.H>
#include <stasher/current.H>

class distrebootObj;
class distrebootBase;
class distreboot_options;

typedef x::ref<distrebootObj, distrebootBase> distreboot;

typedef x::ptr<distrebootObj, distrebootBase> distrebootptr;

// The main class. It's an application singleton.

class distrebootObj : public x::eventqueuemsgdispatcherObj {

	LOG_CLASS_SCOPE;

	// Heartbeat interval setting

	static x::property::value<x::hms> heartbeat_interval;

	// This long after the heartbeat's expiration, it's subject to purging

	static x::property::value<x::hms> stale_interval;

protected:
	stasher::client *client; // On the executing thread's stack

	class serverStatusCallbackObj;

	// The heartbeat object in the repository gets parsed into here.

	class heartbeatObj : virtual public x::obj {

	public:
		// Keys are node names. Value is the epoch time the heartbeat
		// expires, the node is considered to be unavailable at that
		// time.

		typedef std::map<std::string, time_t> timestamps_t;

		// This object's uuid in the repository

		x::uuid uuid;

		// Each node's timestamp

		timestamps_t timestamps;

		// Default constructor
		heartbeatObj();

		// Construct from retrieved object file descriptor
		heartbeatObj(const x::uuid &uuidArg, const x::fd &fdArg);

		// Create a string to go into the repository
		std::string toString() const;

		~heartbeatObj() noexcept {}
	};

	// Some convenience typedefs
	typedef x::ref<heartbeatObj> heartbeat;
	typedef x::ptr<heartbeatObj> heartbeatptr;

	// Current heartbeat object. Use a versionedptr, so that it
	// gets updated by a versioned put.

	typedef stasher::currentObj<heartbeatptr,
				    stasher::versionedptr<heartbeatptr>
				    > currentHeartbeatObj;
	
	typedef x::ref<currentHeartbeatObj> currentHeartbeat;

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

		// Create a string to go into the repository
		std::string toString() const;

		~rebootListObj() noexcept;
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


	currentHeartbeat *heartbeat_info_instance; // On the executing thread's stack
	currentRebootList *rebootlist_instance; // On the executing thread's stack

	void do_update_my_heartbeat();

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
		std::string node;  // --repo

		std::string forcenodename; // Used by regression tests

		argsObj() : start(false), stop(false),
			    reboot(false), cancel(false), dry_run(false) {}
		argsObj(const distreboot_options &opts);

		~argsObj() noexcept 
		{
		}

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(start);
			iter(stop);
			iter(reboot);
			iter(cancel);
			iter(dry_run);
			iter(node);
		}
	};

	typedef x::ref<argsObj> args;
	typedef x::ptr<argsObj> argsptr;

	// Return value from the singleton.

	class retObj : virtual public x::obj {

	public:
		retObj() : exitcode(0) {}
		~retObj() noexcept {}

		std::string message;
		int exitcode;

		retObj(const std::string &messageArg,
		       int exitcodeArg) : message(messageArg),
					  exitcode(exitcodeArg)
		{
		}

		template<typename iter_type> void serialize(iter_type &iter)
		{
			iter(message);
			iter(exitcode);
		}
	};

	typedef x::ref<retObj> ret;

	distrebootObj();
	~distrebootObj() noexcept;

	// The singleton thread.
	ret run(uid_t uid, argsptr &args);

	// Do the actual reboot of this node
	// Punt it to a subclass. The default implementation just stop()s.

	virtual void do_reboot();

#include "distreboot.msgs.all.H"

};

// distreboot::base provides some convenience typedefs.

class distrebootBase : public x::ptrrefBase {

public:

	typedef distrebootObj::args args;

	typedef distrebootObj::argsptr argsptr;

	typedef distrebootObj::ret ret;
};

#endif
