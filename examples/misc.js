SCRIPT_NAME = "misc";
SCRIPT_VER = "1";
SCRIPT_DESC = "examples";

if (nickcmp (get_info ('nick'), 'TingPing') == 0)
	print ('\00320You are cool!');
else
	print (strip('\002\00319You are not cool, no colors for you.', STRIP_COLOR));
