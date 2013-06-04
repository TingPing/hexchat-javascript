
SCRIPT_NAME = "regex";
SCRIPT_VER = "1";
SCRIPT_DESC = "example of regex";

var re = new RegExp(".*(nickserv|ns)\sidentify\s\w+");

function chan_cb (params)
{
	if (params[1].search(re)) // message
	{
		command('say ' + params[0] + get_prefs('completion_suffix') + ' You should change your password.'); // nick
	}
}

hook_print ("Channel Message", chan_cb);
