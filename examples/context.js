SCRIPT_NAME = "context";
SCRIPT_VER = "1";
SCRIPT_DESC = "example of context handling";

// Note this assumes unique channel names across networks

ctx = get_context();
print("Current context is: " + ctx);
old_chan = get_info("channel");

command("query -nofocus (context-testing)");

for each (chan in get_list('channels'))
{
    if (chan.channel == "(context-testing)")
    {
        print("New context is: " + chan.context);
        if (!set_context(chan.context))
            print("Failed to set context!");
        else
            print("This is the new context");
    }
}

old_ctx = find_context("", old_chan);
if (!set_context(old_ctx))
    print("Failed to set context!");
else
    print("This is the old context");