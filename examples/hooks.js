
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

function keypress_cb (keys)
{
	// keys[0] is the key and keys[1] is the modifier (both as strings)
	// modifiers differ on windows and unix
	// see https://developer.gnome.org/gdk/stable/gdk-Event-Structures.html#GdkEventKey for more info
	if (keys[0] == '107' && keys[1] == '4') // ctrl+k
	{
		// You can unload yourself but it must be ran from a timer
		command ('timer 1 unload ' + SCRIPT_NAME + '.js');
	}
}

function unload_cb ()
{
	print("No don't kill me!");
}

hook_command ('test', test_cb, 'there is no help for test =(');
timer_hook = hook_timer (10000, timer_cb); // 10 sec
hook_unload (unload_cb);
hook_special ('Key Press', keypress_cb);
