
SCRIPT_NAME = "time";
SCRIPT_VER = "1";
SCRIPT_DESC = "example of using times";

var date = new Date();
var now = date.getTime();

for each (user in get_list('users'))
{
	let usertime = user.lasttalk.getTime();

	if (now - usertime < 60 * 1000)
		print (user.nick + ' has spoke recently.');
}
