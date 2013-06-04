
SCRIPT_NAME = "hooks";
SCRIPT_VER = "1";
SCRIPT_DESC = "examples of hooks";

var timer_hook;

function test_cb (words)
{
	if (words[1] == 'stop')
	{
		unhook (timer_hook);
		print ('timer unhooked');
	}
	else
		print ("stop calling test it doesn't do anything!");

	return EAT_ALL; // Don't let anything else use /test
}

function timer_cb ()
{
	command ('test');

	return true; // return true to continue the timer
}

function unload_cb ()
{
	print("No don't kill me!");
}

hook_command ('test', test_cb, 'there is no help for test =(');
timer_hook = hook_timer (10000, timer_cb); // 10 sec
hook_unload (unload_cb);
