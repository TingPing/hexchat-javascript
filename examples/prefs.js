
SCRIPT_NAME = "prefs";
SCRIPT_VER = "1";
SCRIPT_DESC = "example of pluginpref";

function pref_cb (word, word_eol)
{
	switch (word[1])
	{
		case "add":
			if (set_pluginpref (word[2], word_eol[3]))
				print (word[2] + ' added');
			break;

		case "delete":
			if (del_pluginpref (word[2]))
				print (word[2] + ' deleted or never existed');
			break;

		case "list":
			print (list_pluginpref ());
			break;

		case "get":
			print (get_pluginpref (word[2]));
			break;

		default:
			command ('help pref');
	}
}

hook_command ('pref', pref_cb, "USAGE: pref add <var> <val>\n           delete <var>\n           get <var>\n           list");
