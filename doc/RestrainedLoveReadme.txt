This is the edited version of the original readme file by Marine Kelley.
The file has been edited to take into account the improvements by Henri Beauchamp, as well as the Cool VL Viewer.

-------------
Version 2.09.29.28


WHAT IS IT ?
------------

RestrainedLove is aimed at BDSM fans in SL who wish to enhance their experience by letting other people (such as their owners) take control of some of their abilities. In order to use its features, the Dom/me has to operate items (typically restraints) that the sub wears, but only the sub needs to use this viewer in that situation. Here is what a Dom/me can do to a sub who uses this viewer with RestrainedLove enabled:

- Make an item undetachable (once locked, the sub has absolutely no way to detach it unless they relog with a different viewer or the item is unlocked).
- Prevent sending IMs, receiving IMs, sending chat or receiving chat (with exceptions if needed).
- Prevent teleporting (from the map, a landmark, or by a friend with exceptions if needed).
- Prevent rezzing, editing, using inventory, reading notecards, sending messages on non-public channels (again with exceptions if needed).
- Prevent standing up and force sitting.
- Prevent adding/removing clothes, force removing clothes and force detaching worn items (unless made undetachable).
- Force attaching clothes and items that are "shared" in the user's inventory (see below)
- Force teleporting the sub to an arbitrary location, without the ability to either refuse or cancel the teleport
- Hide names and/or location so that the sub cannot know who is around, or where they are

These features, when cleverly used together, make the sub truly *feel* the power of their Dom/me. Tested and approved by a lot of slaves.

Lockable items in SL work perfectly without this viewer but if you use it you'll find your experience... enhanced. But you don't *need* it to use them. Moreso, the Dom/me does not even need to use it at all, since it is made to enhance restraints (which are worn by the sub).



HOW TO ENABLE RESTRAINEDLOVE ?
---------------------------

- Get the Cool VL Viewer from http://sldev.free.fr/ for Linux or Windows, or from the link to be found in the announcement forum (http://sldev.free.fr/forum/viewforum.php?f=3) for MacOS X.
- Install the Cool VL Viewer following the instructions given on the download site.
- Start the viewer and log in.
- Open the preferences menu of the viewer.
- Select the "Cool features" tab.
- Select the "RestrainedLove" sub-tab.
- Check the "RestrainedLove mode" checkbox in this sub-tab.
- Select your preferred profile and settings in this sub-tab.
- Close the menu with "OK".
- Log off and quit the viewer.

Now, your viewer is RestrainedLove enabled and you may log on again.



A WORD ABOUT SHARED FOLDERS
---------------------------

Read on, and also visit http://realrestraint.blogspot.com/2008/08/objects-sharing-tutorial.html for a tutorial explaining how to share folders properly, both with mod and no-mod objects.

Since v1.11, the viewer can "share" (more like "expose", see below) some of your items with scripts in world in order to let them force you to attach, detach and list what you have shared.

RL viewer v1.13 or above is able to share multiple levels of sub-folders to facilitate organization.

"Share" does NOT mean they will be taken by other people if they want to (some of the items may be no-transfer anyway), but only that they can force YOU to wear/unwear them at will through the use of a script YOUR restraints contain. They will remain in your inventory.

To do this:
* Create a folder named "#RLV" (without the quotes) directly under "My Inventory" (right-click on "My Inventory", select "New Folder"). We'll call this folder the "shared root".
* Move a folder containing restraints or other attachments directly into this new folder.
* Wear the contents of that folder, that's it !

So it would look like this:

 My Inventory
 |- #RLV
 |  |- cuffs
 |  |  |- left cuff (l forearm)
 |  |  \- right cuff (r forearm)
 |  \- gag
 |     \- gag (mouth)
 |- Animations
 |- Body Parts
 .
 .
 .

For example: If you're owning a set of RR Straps and want to "share" (expose) them, just move the folder "Straps BOXED" under the shared root.

Either wear all the items of the folders you have just moved (one folder at a time !) or rename your items yourself, so that each item name contains the name of the target attachment point. For example: "left cuff (l forearm)", "right ankle cuff (r lower leg)". Please note that no-modify items are a bit more complex to share, because they cannot be renamed either by you or by the viewer. More on that below.

The attachment point name is the same as the one you find in the "Attach To" menu of your inventory, and is case insensitive (for example: "chest", "skull", "stomach", "left ear", "r upper arm"...). If you wear the item without renaming it first it will be renamed automatically, but only if it is in a shared folder, and does not contain any attachment point name already, and is mod. If you want to wear it on another attachment point, you'll need to rename it by hand first.

Pieces of clothing are treated exactly the same way (in fact they can even be put in the folder of a set of restraints and be worn with the same command). Shoes, for instance, are a good example of mixed outfits: some attachments and the Shoes layer. Clothes are NOT renamed automatically when worn, since their very type decides where they are to be worn (skirt, jacket, undershirt...).

HOW TO SHARE NO-MODIFY ITEMS:
As you already know, no-mod items cannot be renamed so the technique is a bit more complex. Create a sub-folder inside the outfit folder (such as "cuffs" in the example above), put ONE no-modify item in it. When wearing the object, you'll see the folder itself be renamed (that's why you must not put more than one object inside it). So if your outfit contains several no-mod objects, you'll need to create as many folders and put the no-mod objects in them, one in each folder.

Example with no-modify shoes:

 My Inventory
 |- #RLV
 |  \- shoes
 |     |- left shoe (left foot)
 |     |  \- left shoe   (no modify) (no transfer)  <-- no-mod object
 |     |- right shoe (right foot)
 |     |  \- right shoe   (no modify) (no transfer) <-- no-mod object
 |     \- shoe base   (no modify) (no transfer)     <-- this is not an object
 |- Animations
 |- Body Parts
 .
 .
 .

GOTCHAS:
* Do NOT put a comma (',') in the name of the folders under the shared root or it would screw the list up.
* Don't forget to rename the items in the shared folders (or to wear these items at least once to have them be renamed automatically) or the force attach command will appear to do nothing at all.
* Avoid cluttering the shared root (or any folder under it) with many sub-folders, since some scripts may rely on the list they got with the @getinv command and chat messages are limited to 1023 characters. Choose wisely, and use short names. But with 9 characters per folder name average, you can expect to have about 100 folders available.
* Remember to put no-modify items in sub-folders, one each, so their names can be used by the viewer do find out where to attach them. They can't be shared like modify items since they can't be renamed, and the outfit folder itself will not be renamed (since it contains several items).



WHAT DOES IT DO IN DETAIL ?
---------------------------

Let's go down to business. There are 2 kinds of modifications:

* Permanent modifications. These are the ones that apply all the time, whether you're wearing a locked item or not:
- You cannot choose your log in location (it always log you in your last location) when you were TP-restricted on last logout.
- Automatic IM answer to anybody who sends "@version" (lowercase) to you, the viewer will answer its version so it's a quick way to check if a sub is using it or not. Note: some uneducated people send unsolicited "@version" IMs to check other people's viewers, without saying "Hi" or anything else. People using regular viewers do see these unsolicited IMs and associate them with spam, possibly going as far as ARing the initiator. I cannot be held responsible for that, just don't do this to people you don't know.

* Global modifications: These are the ones that occur when an object triggers them, modifying the global behavior of the viewer, such as:
- No IM
- No chat
- No read notecards
- etc...

* Temporary modifications. These are the ones that occur when an object is locked on your avatar.
- No HUD attachments hiding. If you don't know what I mean don't ask, but that's a way to cheat restraints that prevent you from interacting with your environment, you can't cheat here.
- No Wireframe view (same remark as above).
- No "Dump All Attachments" (same remark as above).
- No "Attach To" or "Wear" when right clicking on an object in world. That could kick a locked object otherwise (see below for v1.21 and v1.23).
- "Edit" > "Detach Object..." in the top menu bar is not working on locked objects.
- The "Tools" > "Release Keys" item in the top menu bar is inactive.
- No Drag-and-drop of objects and folders from your inventory to your avatar.
- No "Detach and Drop" on the Pie menu (right click on it in-world).
- No "Detach" on the Pie menu when right clicking on your avatar.
- No "Detach All" on the Pie menu when right clicking on your avatar.
- "Attach To" from the inventory: if you try to attach on an attach point that contains a locked object will do nothing. It works for other attach points though, of course.
- No Detach from Yourself menu item in the inventory on locked objects.
- No "Tools" > "Reset/Recompile" scripts in selection on locked objects.
- No New Script on a locked item.
- No "Add to Outfit", "Take Off Items" from the folders menu actions while wearing a locked item as they do the same thing as "Wear" (see below).
- No "Wear" menu item in the inventory when you're wearing at least one locked object (*)
- No attach/detach by double-clicking on objects in the inventory (Cool VL Viewer feature).
- No modifying the inventory of a locked item

(*): a bit harsh indeed... but it's the only solution for design reasons (regular viewer design that is). When trying to Wear an object, the viewer does not know where to attach the item so it waits for the server to send an "attach there" message, and the viewer has no choice but to comply. You may "Attach To" instead but be very careful with it: know where your object has to attach first or you'll end up having to reposition it by hand. It might be a good idea to rename the objects (when they are mod-ok) so that their normal attachment point is appended to their name.

Since 1.11 you are able to use Wear again if the name of the object contains the name of an attachment point (named like the ones in the "Attach To" sub-menu). This fake Wear is actually a disguised Attach To command.

Since v1.21a you may set the RestrainedLoveAllowWear advanced setting to TRUE so to allow the Wear command again on all items. In this case, the RestrainedLove code will attempt to reattach automatically any kicked off locked item. However, be warned that because of a serious bug in the asset server (see https://jira.secondlife.com/browse/SVC-3579), attachments that are re-worn too fast (less than 15 seconds or so, but up to 30 seconds, depending on asset servers lag) after being detached will loose all the modifications (including script states) that were made to them after the last time they were attached or the last time you TPed or logged in with them on (whichever occurred last). Starting from RestrainedLove v1.22f, you may use the RestrainedLoveReattachDelay advanced setting (defaults to 15s) to adjust the reattach delay.

Since v1.23d you may set the RestrainedLoveAddReplace advanced setting to TRUE so to allow the "Add to Outfit" and "Replace Outfit" commands again on all outfit folders.


RELEASE NOTES:
--------------

2.09.29.28 (@versionnum = 2092928) by Henri Beauchamp:
- Fixed: rendering bugs related with vision limiting spheres, introduced with the performance viewer code.


2.09.29.27 (@versionnum = 2092927) by Henri Beauchamp:
- Added: allow to prefix with #RLV/~ paths passed as command options, since this is allowed by RLVa and used by some non-strict-RLV-conforming scripts...


2.09.29.26 (@versionnum = 2092926) by Henri Beauchamp:
- Improved: the no-mod attachements folder renaming in #RLV/ was made much smarter. Only folders bearing the default "New Folder" name (i.e. freshly created folders) are ever auto-renamed, and if the folder contatining the no-mod item does not match this rule, a new folder with the appropriate joint name is created instead and the no-mod attachement item is moved into it. This should avoid confusing behaviours observed by users unaware of this auto-renaming peculiarity, and pretty much makes the "RestrainedLoveAutomaticRenameItems" debug setting disabling by these users useless.

2.09.29.25 (@versionnum = 2092925) by Henri Beauchamp:
- Inclusion of Marine Kelley's change:
	- Added: a new "RestrainedLoveAutomaticRenameItems" debug setting allows to disable (when set to FALSE) the auto-renaming (or moving to a sub-folder, for no-mod items) of attachements contained in #RLV. A new "Add joint name to attachments in #RLV/" entry was added in the "Advanced" -> "RestrainedLove" menu to easily change this setting (defaulting to on).


2.09.29.24 (@versionnum = 2092924) by Henri Beauchamp:
- Fixed: when under @shownames restriction, do not open the avatar profile floater when clicking on the censored name link in the Object IM info floater.


2.09.29.23 (@versionnum = 2092923) by Henri Beauchamp:
- Inclusion of Marine Kelley's change:
	- Fixed: could not @attach an item located inside a nested temporary folder (ex: "#RLV/~a/b/c").


2.09.29.22 (@versionnum = 2092922) by Henri Beauchamp:
- Added: a "RestrainedLoveRelaxedTempAttach" setting ("@acceptpermission allows temp-attachments" toggle in "Advanced" -> "RestrainedLove" menu), to allow temp-attachments to bypass the attach permission dialog when @acceptpermission is in force. Defaults to FALSE given the dangerous aspect of this feature...


2.09.29.21 (@versionnum = 2092921) by Henri Beauchamp:
- Inclusion of Marine Kelley's change:
	- Changed: giving a folder to #RLV will now create nested sub-folders if the name of the folder contains slashes ("/").
- Fixed: Marine's code which AISv3 did not like at all... Now using proper folder moving and renaming to make the server happy.


2.09.29.20 (@versionnum = 2092920) by Henri Beauchamp: (equivalent to Marine's v2.09.29.00):
- Fixed: a crash seen happening when using @camtextures or @setcam_textures and the texture specified is not used elsewhere in world.
- Fixed: implemented the missing @shownames_sec support.
- Fixed: do not update the speakers list panels, when under @shownames restriction.
- Fixed: in the "RestrainedLove status" floater, do properly list the exceptions added via @*_sec command variants.
- Added: when not in EE rendering mode, do allow @getenv_sunimage, @getenv_moonimage and @getenv_cloudimage to return the corresponding Windlight default image UUID.
- Changed: do not block any more the sending of the "agent typing" event when under @redirchat restrictions.
- Inclusion of Marine Kelley's changes:
	- Fixed: sometimes, moving the camera at certain angles while not rendering the avatar would not render the vision spheres either.
	- Fixed: prevent llDialog/llTextBox targetting channel 0 while @sendchat or @redirchat are active.
	- Fixed: looking at the Experiences floater would show the slurls even when under @showloc.
	- Added: new RLV command @shownearby (thank you Chorazin Allen for the code).
	- Added: new RLV command @touchattachother:UUID to prevent from touching a specific avatar's attachments.
	- Added: new RLV command @share (with its _sec variant as well as @share:UUID exceptions) to prevent the user from giving anything to anyone, with exceptions.
	- Added: new RLV command @sitground to force the avatar to sit on the ground where it stands (in fact it will just sit where it is, even if it is flying, it's not necessarily going to sit on the ground below).
	- Added: new notification when sitting and unsitting on/from an object or the ground: the viewer says "/sat object legally", "/unsat ground legally" etc.
	- Added: new RLV command @editworld to prevent the user from editing objects rezzed in-world but not their own attachments (no exceptions).
	- Added: new RLV command @editattach to prevent the user from editing their own attachments but not objects rezzed in-world (no exceptions).
	- Added: new RLV command @sendim:<group_name> to add exceptions to @sendim by group. If <group_name> is "allgroups", then all the groups are exceptions.
	- Added: new RLV command @recvim:<group_name> to add exceptions to @recvim by group. If <group_name> is "allgroups", then all the groups are exceptions.
	- Added: new RLV command @sendimto:<group_name> to prevent from sending IMs to a specific group. If <group_name> is "allgroups", then all the groups are restricted.
	- Added: new RLV command @recvimfrom:<group_name> to prevent from receiving IMs from a specific group. If <group_name> is "allgroups", then all the groups are restricted.
	- Added: a slew of new @setenv_* and @getenv_* RLV commands, some of them to match the new ones in Catznip: asset, cloudtexture, sunscale, suntexture, sunazimuth, sunelevation, moonazimuth, moonelevation, moonbrightness, moonscale, moontexture.
	- Added: support for vectors in the following "setenv/getenv" commands: ambient, bluedensity, bluehorizon, cloudcolor, cloud, clouddetail, cloudscroll, sunlightcolor. Attention: although ";" and "/" are acceptable as separators in the "setenv" command, the "getenv" command will separate the numbers with ";" only to remain consistent with other RLV commands ("/" is supposed to be for folders, it is not a separator).


2.09.28.20 (@versionnum = 2092820) by Henri Beauchamp (equivalent to Marine's v2.09.28.03):
- Inclusion of Marine Kelley's changes:
	- Changed: mixing camdrawcolor is now a simple multiply between the different colors. It used to be a HSL mix but when two hues were too far apart, the resulting mix would give a totally different hue, which was confusing. Ex: hue 0 (red) mixed with hue 250 (purple-red) would give a green-blueish hue while it should be red. Wrapping the hue when the difference is >180° would do the trick, but I find it is more intuitive to do a simple multiply (that way we are guaranteed that the more colors, the darker the result).
	- Changed: allow to activate "highlight invisible" even when the vision is restricted, but in this case, make the inner vision sphere opaque to avoid cheating that way.
	- Changed: allow to see the selection silhouettes even when the vision is restricted, but in this case and if the "show selection outlines" option is active, make the inner vision sphere opaque to avoid cheating that way.
	- Fixed: when the avatar is a cloud (or when the camera does not render the avatar), non black or white semi-transparent vision spheres are not rendered the same and may allow to cheat by seeing better than we should.
	- Fixed: when doing a drag-and-drop of an inventory object to an in-world object, make the vision spheres opaque to avoid cheating by looking at the highlights.
	- Fixed: don't send automatic IM responses to @version, @getlist etc to someone who is blocked.
	- Fixed: we couldn't open the Build window with the Build button or Ctrl+B, but we could with Ctrl+3..


2.09.27.23 (@versionnum = 2092723) by Henri Beauchamp:
- Fixed: the bug that caused impossibility to edit or open objects on which an @edit exception was placed when @edit restriction is in force.
- Improved: in the "RestrainedLove status" floater commands log, list the implicit/internal @end-relay command instead of a space, with the new "Implicit" status (instead of "Executed").
- Improved: in the "RestrainedLove status" floater, renamed the first tab as "Objects status" (contains the full list of per-object RLV commands in force, like before), and added a second "Restrictions" tab with just actual restrictions commands listed and an "Exception" column (with the exceptions/relaxations to the restriction, including UUID to name resolving for attachments, group and avatar names).
- Improved: @setenv_preset:option=force now applies any preset matching the name passed as 'option' (ignoring case), be it an inventory setting, a Windlight setting, sky, day or water setting (in this order of preferences). When successfully loaded, the preset will be converted to EE settings and Windlight overriding is enabled if it was not in force already.
- Added: an extension to RLV's API for @setenv_preset, allowing to choose what type of setting to search for (among 'sky', 'day' and 'water' presets): e.g. @setenv_preset:sky|blizzard=force will search for "blizzard" in sky settings only. An especially useful case is when a setting name is shared by several types, like "Default", e.g. @setenv_preset:day|default=force will load the default day setting while @setenv_preset:water|default=force will load the default water setting.


2.09.27.22 (@versionnum = 2092722) by Henri Beauchamp:
- Added: @setenv_daytime support for the Extended Environment renderer.
- Fixed: do not allow the use of Lua environment settings related functions when under @setenv restrictions.
- Fixed: loading of Windlight sky presets (via @setenv_preset) with capitals in their name.


2.09.27.21 (@versionnum = 2092721) by Henri Beauchamp:
- Improved: in the "RestrainedLove status" floater, auto-scroll to end of commands log when new commands are logged.
- Added: a new "RestrainedLoveLuaNoBlacklist" debug setting (defaulting to FALSE) to optionnaly allow viewer-side Lua scripts (but *not* Lua commands relayed from objects) to bypass the commands black list. This can be toggled with the "Advanced" -> "RestrainedLove" -> "Skip blacklist checks for Lua scripts" menu entry.
- Fixed: do not force shaders on when using @setdebug. Instead, force them on (as long as allowed) only when @setdebug_RenderResolutionDivisor is used.
- Fixed: do not activate environment settings via @attach=force-alike commands when "RestrainedLoveNoSetEnv" is TRUE.


2.09.27.20 (@versionnum = 2092720) by Henri Beauchamp (equivalent to Marine's v2.09.27.00):
- Improved: added a commands log tab to the RestrainedLove floater (now renamed "RestrainedLove status"), which shows all the RLV commands, their originator and their status. The "Advanced" -> "RestrainedLove" -> "Restrictions and command log" menu entry opens this floater (as long as the @viewscript restriction is not in force).
- Changed: removed the "RestrainedLoveDebug" debug setting and the associated "Advanced" -> "RestrainedLove" -> "Debug mode" menu entry (you can use the new logging facility instead, and you still can enabled the "RestrainedLove" tag in the "Advanced" -> "Consoles" -> "Debug tags" floater, to log more RLV stuff into the viewer log file for a deeper debugging).
- Fixed: a few bugs in restrictions enforcemnent on open floaters.
- Fixed: a bug in @getenv_preset that always returned an empty string.
- Added: support for Enhanced/Extended Environment with @setenv_* and @getenv_* commands. The experimental Cool VL Viewer v1.27.0 also got additional commands of that type, as implemented in Marine's EE RLV (e.g. @[s|g]env_[sunazim|sunelev|moonazim|moonelev|cloudvariance|moisturelevel|dropletradius|icelevel|sunimage|moonimage|cloudimage]).
- Added: activation of Enhanced/Extended Environment settings with @attach=force and similar commands, as long as Windlight mode is not in force.
- Inclusion of Marine Kelley's changes:
	- fixed: wrong disabling of joystick camera overriding.
	- changed: removed the RestrainedLoveCamDistNbGradients debug setting to avoid cheating.
	- improved: black spheres rendering when vision is restricted.


2.09.25.20 (@versionnum = 2092520) by Henri Beauchamp (equivalent to Marine's v2.09.25.03):
- Inclusion of Marine Kelley's changes:
	- fixed : Alpha-blended rigged attachments are no longer rendered beyond the vision restriction spheres at all, and this time it does not distinguish between materials, non-materials, fullbright, shiny and all that stuff.
	- improved : Optimized the rendering of the vision restriction spheres.
	- changed: Removed the artificial fartouch restriction when our vision is restricted, because there are cases where we might want to be able to touch something beyond our visual range.


2.09.24.21 (@versionnum = 2092421) by Henri Beauchamp:
- Added: Lua callbacks for RLV command handling and answers on chat.
- Added: support for Universal tattoo wearable (when compiled in the viewer).


2.09.24.20 (@versionnum = 2092420) by Henri Beauchamp (equivalent to Marine's v2.09.24.00):
- Inclusion of Marine Kelley's changes:
	- fixed : The render complexity debug display was not censored under @shownames.
	- fixed : Hearing emotes said by a personal attachment which imitated the name of an avatar around us, did not have its name obfuscated when under @shownames.


2.09.23.20 (@versionnum = 2092320) by Henri Beauchamp (equivalent to Marine's v2.09.23.00):
- Inclusion of Marine Kelley's changes:
	- improved : when rendering avatars as silhouettes via the @camavdist RLV command, the silhouettes now appear as flat grey 2D without any shading, as they were intended since the beginning.
	- improved : silhouettes now feature every single attachment the avatar is wearing, including unrigged ones unlike before. The only attachments that still don't show are the unrigged alpha-blended ones. I will try to work on that in the future.
	- improved : silhouettes faces are now hidden on a face-by-face basis and not on an object basis, i.e. you should no longer see alpha-blended parts of an avatar disappear when taking a step back while blindfolded.
	- fixed : censoring names (replacing with "That resident", "This person" etc) in the chat and in HUD texts looked wonky when someone had a very short user or display name. For example, hearing someone say "I need more money" no longer reads as "I need This soulre This soulney" when around Mo Noel.
	- fixed : glow and fullbright no longer poke through the vision restriction spheres when they are not black.
	- fixed : surfaces that are fullbright with environment and materials and alpha-blended no longer render through the vision restriction spheres.


2.09.22.20 (@versionnum = 2092220) by Henri Beauchamp (equivalent to Marine's v2.09.22.00):
- Inclusion of Marine Kelley's changes:
	- fixed : HUD hovertexts were visible on snapshots when the vision was restricted, but only when looking towards 0, 0, 0, regardless of the distance.
	- fixed : do not allow to file an abuse report or a bug report when under @showloc, as it discloses the location (and if we hid it on those windows, it would decrease the quality of the reports and waste LL's time).
	- fixed : when receiving a send chat restriction while typing, the "Typing" tag would never disappear until the restriction was lifted.
- Changed: added a notification to explain why the abuse report does not work while prevented to see names or locations.


2.09.21.24 (@versionnum = 2092124) by Henri Beauchamp (equivalent to Marine's v2.09.21.03):
- Inclusion of Marine Kelley's changes:
	- fixed : @detachme wasn't working when called from a child prim.
	- fixed : reverted the "fix" added a few versions ago to account for llUnSit() being called on an avatar prevented from standing up, which caused a race condition (and an endless switch of seats) when executing: @sit:<uuid>=force,unsit=n while the avatar is already sitting.
- Changed: further improved the RLV command execution and queuing logic to prevent other race condition issues between @(un)sit=force and @unsit=n commands.
- Changed: do not try and enforce @standtp on a parcel with teleport routing set (since it would fail or TP the avatar to the landing point), and warn the agent (non-modal alert shown only once per session and per parcel) when they have TP routing changing rights on this parcel (so that they can take action).


2.09.21.23 (@versionnum = 2092123) by Henri Beauchamp:
- Changed: rewrote the whole RLV command execution and queuing logic to try and prevent race condition issues encountered with some scripts sending opposite commands at in the same command line (e.g. @unsit=force,sit:<uuid>=force).
- Changed: never delay/queue @getcommand till the avatar gets baked: just like @version* commands, that command is likely to be sent at attachment rezzing time, as a form of "ping" message and to discover RestrainedLove features available in the viewer, and we can't afford risking having such an attachment timing out on us because of slow avatar baking.


2.09.21.22 (@versionnum = 2092122) by Henri Beauchamp:
- Changed: RestrainedLove commands (but @version* ones) are now queued and delayed whenever they are received while the avatar is being baked.
- Fixed: an infinite loop bug in fireCommands().
- Improved: some speed optimizations.


2.09.21.21 (@versionnum = 2092121) by Henri Beauchamp (equivalent to Marine's v2.09.21.02):
- Inclusion of Marine Kelley's change:
	- changed : Do not take anymore "?" in emotes as an indication that the user wants to cheat @sendchat.
- Fixed: a bug that could cause the @standtp restriction removal to fail on @clear (when the latter is received while the avatar is still sitting).


2.09.21.20 (@versionnum = 2092120) by Henri Beauchamp (equivalent to Marine's v2.09.21.01):
- Inclusion of Marine Kelley's change:
	- fixed : @sendchannel would block @version, @versionnum and all the other RLV messages sent to a script (thank you Chorazin Allen for the heads-up).
- Fixed: the bug in the above fix, which limited sent text to 255 characters for positive channels (like what happens for negative channels, which can't be avoided). Recoded it to allow up to 1023 characters for positive channels.


2.09.20.23 (@versionnum = 2092023) by Henri Beauchamp (equivalent to Marine's v2.09.20.03):
- Inclusion of Marine Kelley's change:
	- changed : When an object issues a @tpto RLV command to force-TP the avatar somewhere, ignore @tplocal restrictions issued by that object.


2.09.20.22 (@versionnum = 2092022) by Henri Beauchamp (equivalent to Marine's v2.09.20.02):
- Inclusion of Marine Kelley's changes:
	- changed : Prevent from putting an object into a box if it belongs to a locked folder, because it could be brought back to the inventory into another folder which might not be locked.
	- changed : When under @unsit and forced to stand up by a llUnSit() LSL call, force to sit back to where we were (only works for prims with a defined sit target, like a scripted furniture or a poseball).
	- fixed : A gesture sending /77arm bypassed @sendchannel_sec=n (thank you Daisy Rimbaud for the heads-up).
- Fixed a bug that prevented @setdebug_renderresolutiondivisor to work.
- Large code cleanup and speed optimizations.


2.09.20.21 (@versionnum = 2092021) by Henri Beauchamp:
- Fixed: a bug in the new @setcam_fov* commands support, which prevented from focusing the camera far away while no camera constraint was in force.


2.09.20.20 (@versionnum = 2092020) by Henri Beauchamp (equivalent to Marine's v2.09.20.01):
- Inclusion of Marine Kelley's changes:
	- added : @findfolders (find several folders at the same time).
	- added : @touchhud (prevent from touching any HUD).
	- added : @interact (prevent touching anything in-world as well as editing and sitting).
	- added : @getdebug RestrainedLoveForbidGiveToRLV, RestrainedLoveNoSetEnv, WindLightUseAtmosShaders.
	- added : @tprequest (prevent from sending a TP request to someone else).
	- added : @accepttprequest (force send a TP offer to whoever requests one).
	- added : @sharedwear, @sharedunwear (counterparts of @unsharedwear and @unsharedunwear).
	- added : @touchfar, @fartouch with max distance parameter.
	- added : @sittp with max distance parameter.
	- added : @tplocal with max distance parameter.
	- added : @sendchannel_except (prevent from using one particular chat channel).
	- added : @detachthis:uuid, detachallthis:uuid
	- added : @remattach:uuid (force detach an attachment by its UUID).
	- added : @shownames:uuid (hide the names of everyone around except one particular avatar).
	- added : @tpto:region/x/y/z (lookat doesn't work yet) (user-friendlier variant of @tpto).
	- added : @setcam_avdistmin (synonym to @camdistmin).
	- added : @setcam_avdistmax (synonym to @camdistmax).
	- added : @setcam_unlock (synonym to @camunlock).
	- added : @setcam_fov (force the FoV in radians).
	- added : @setcam_fovmin (set the minimum FoV).
	- added : @setcam_fovmax (set the maximum FoV).
	- added : @setcam_textures:uuid (synonym to @camtextures and replaces the grey texture by a custom one).
	- added : @getcam_textures (get the specified UUID).
	- added : @getcam_XXX, XXX being avdistmin, avdistmax, fovmin, fovmax, zoommin, zoommax, zoom, fov.
	- added : @sendgesture.
	- fixed : a bug that would not hide emotes coming from an attachment when it took the same name as its wearer (for example, a gag), while under @shownames and @recvemotes AND "Show ..." was active. Specific bug ! Thank you Katinka Teardrop and Isabel Schulze for the heads-up.
- Changed: @tpto to accept "region/x/y/z;lookat" ("lookat" may be whatever string/numbers) and causes the TP to preserve on arrival the direction your avatar was facing on departure.
- Fixed: removed spurious "attached legally" notifications corresponding to attachments rezzing on *other* avatars.
- Code cleanup, optimizations and various bug fixes.


2.09.19.21 (@versionnum = 2091921) by Henri Beauchamp (equivalent to Marine's v2.09.19.02):
- Inclusion of Marine Kelley's change:
	- fixed : recently introduced bug causing the impossibility to touch anything while under the @edit restriction.


2.09.19.20 (@versionnum = 2091920) by Henri Beauchamp (equivalent to Marine's v2.09.19.01):
- Changed: do not mess with picking rules through transparent HUDs unless there are locked HUDs.
- Inclusion of Marine Kelley's changes:
	- fixed : Prevent click-dragging when @edit is active.
	- fixed : @touchme did not allow you to touch the object issuing this command when you were under @touchall (thanks Kyrah Abattoir for the heads-up).
	- added : If someone types "@list" in an IM with you, they get the list of RLV restrictions you are currently under.
	- added : If someone types "@stopim" in an IM with you and you are under the "@startim" RLV restriction (preventing you from starting IMs), your IM window is closed automatically and both of you get feedback. If you are not under a "@startim" restriction, then the other party gets feedback too to show that it didn't work.


2.09.16.21 (@versionnum = 2091521) by Henri Beauchamp:
- Fixed a couple of bugs dealing with inventory items force attached/detached to/from the "Root" joint.
- Changed: use AIS when possible for renaming categories and items.


2.09.16.20 (@versionnum = 2091620) by Henri Beauchamp (equivalent to Marine's v2.09.16.00):
- Fixed a bug that prevented the "attached legally" @notif'icaction to be emitted when adding an attachment on an already occupied joint.
- Inclusion of Marine Kelley's changes:
	- changed : Prevent activating wireframe view only when there's a HUD locked, or when our vision is restricted (unlike before, when wireframe was prevented as soon as at least one object was locked).
	- fixed : Made it so we can now move, rotate and resize HUDs while the vision is restricted.


2.09.15.21 (@versionnum = 2091521) by Henri Beauchamp:
- Fixed one more (old) bug in initial outfit restoration under wearing constraints.


2.09.15.20 (@versionnum = 2091520) by Henri Beauchamp (equivalent to Marine's v2.09.15.00):
- Inclusion of Marine Kelley's changes:
	- fixed: friendship requests text messages could go through a @sendim restriction (but not through a @recvim one).
	- fixed: it was possible to join a new group and be set to that group even under @setgroup. Thank you Vanilla Meili for the head-up !


2.09.14.20 (@versionnum = 2091420) by Henri Beauchamp (equivalent to Marine's v2.09.14.00):
- Fixed: better avatar names censorship under @shownames restriction.


2.09.11.22 (@versionnum = 2091122) by Henri Beauchamp:
- Fixed even more (old) bugs in initial outfit restoration under wearing constraints.
- Changed: @setdebug_* does not cause the changed variables to be saved on logoff any more.


2.09.11.21 (@versionnum = 2091121) by Henri Beauchamp:
- Fixed (old) bugs in initial outfit restoration under wearing constraints and notification failures when adding folders containing wearables to an outfit.


2.09.11.20 (@versionnum = 2091120) by Henri Beauchamp (equivalent to Marine's v2.09.11.00):
- Inclusion of Marine Kelley's change:
	- fixed : We could see the beacons through the vision restrictions. Thank you Danna Pearl for the heads-up !


2.09.09.21 (@versionnum = 2090921) by Henri Beauchamp:
- By popular request (1 user), made the configurable blocked IMs auto-replies per-account settings.


2.09.09.20 (@versionnum = 2090920) by Henri Beauchamp (equivalent to Marine's v2.09.09.00):
- Fixed (old) bugs in some RestrainedLove specific tests that were hidden by gcc's -Wno-parentheses option...


2.09.08.20 (@versionnum = 2090820) by Henri Beauchamp (equivalent to Marine's v2.09.08.00):
- Nothing changed !... Just bumped the version number to reflect the fact that 2.09.06.23 was *already* on par with Marine's v2.09.08.00 (an beyond... Marine's still got the AHH @adjustheight wrong).


2.09.06.23 (@versionnum = 2090623) by Henri Beauchamp (equivalent to Marine's v2.09.06.08):
- Inclusion of Marine Kelley's changes:
	- added : A new "Blindfold point of view" sub-menu under "Advanced" -> "RestrainedLove", to change the body joint from which the vision spheres (the black spheres) are rendered, and around which the camera is restricted, instead of just the head. You can now "see" the world around any of your body extremities (head, pelvis, hands and feet). The point of view is always the head when in Mouselook however.
	- changed : Allow to see the body attachments that have alpha textures from a little further away. We now take into account the size of the attachment instead of just its position. It's not perfect but it works well.


2.09.06.22 (@versionnum = 2090622) by Henri Beauchamp (equivalent to Marine's v2.09.06.07):
- Inclusion of Marine Kelley's changes:
	- fixed : We could edit uneditable objects with a little trick on the user interface (thanks Mo Noel for the heads-up).
	- fixed : Name tags were disappearing when forced into Mouselook.
	- added : A hack to force every object around to be "refreshed", as if they just appeared for the viewer. This is especially useful when blindfolded and attachments worn by other avatars are not visible (because they are semi-transparent and that's hidden beyond the vision spheres) and were not refreshed automatically. The menu entry is "Advanced" -> "RestrainedLove" -> "Refresh visibility of objects".


2.09.06.21 (@versionnum = 2090621) by Henri Beauchamp (equivalent to Marine's v2.09.06.06):
- Fixed: Do not cause non-movable objects you do not own but are rezzed on your land to be non-returnable via the pie menu.
- Changed the way the RestrainedLove Debug mode works for logging: enabling/disabling that mode now also adds/removes the "RestrainedLove" debug tag from the logger, toggling on/off debug-level messages for RestrainedLove in the viewer log instead of info-level messages.
- Inclusion of Marine Kelley's changes:
	- fixed : When @standtp is lifted, no longer TP back to the last location after standing up (wherever we may be).
	- fixed : @standtp should be consistent over relogs now.
	- fixed : Alpha blended textures on rigged mesh attachments without materials (such as rigged hair) would render through the vision spheres.
	- fixed : Hide the selection particle beam of other avatars when the vision is restricted.
	- fixed : Hide cross sections and grids when editing an object while the vision is restricted.
	- fixed : Unrigged alpha attachments were visible through the vision spheres.
	- fixed : Show point at was still visible when the vision was restricted.
	- fixed : Semi-transparent attachments worn by other avatars would show through the vision spheres if they had materials. Now they are rendered only when close enough, like particles.
	- fixed : Editing and highlighting objects could make them render through the vision spheres, now there is no highlighting anymore when the vision is restricted (but you can still edit stuff, just... less easily).
	- fixed : Removed a loophole that could allow the user to edit an object even when @edit was active, under some very specific conditions.
	- fixed : When the vision is restricted, don't let the user capture the depth buffer on snapshots.
	- fixed : The viewer was slowed down by some unnecessary object updates when receiving or removing restrictions (it was noticeable when receiving bursts of RLV commands), even those that had nothing to do with vision restrictions.


2.09.06.20 (@versionnum = 2090620) by Henri Beauchamp (equivalent to Marine's v2.09.06.01):
- Fixed: do not allow to track avatars when under @shownames/@shownamestags or landmarks when under @showminimap/@showorldmap restrictions.
- Inclusion of Marine Kelley's changes:
	- fixed : When stuck in mouselook, we couldn't move the cursor and the mouse at the same time.
	- fixed : When issuing @detachallthis without an option and without @detachthis along with it, we could detach the object from the pie menu (not from the inventory), and we could unwear pieces of clothing contained in the same folder (but not in child folders).
	- changed : When forced to sit somewhere, allow to do it even when @sittp is active, to let traps re-capture the avatar after a relog.
	- changed : When the vision is restricted, force all "Rendering Metadata" types to true, forbidding from hiding the world, the prims, and more importantly, the avatars (since the vision spheres now need the user's avatar to render).
	- fixed : Now particles and alpha attachments are rendered correctly when the vision is restricted. This implied changing the place where the rendering of the spheres was called, which brought its own lot of bugs, which in turn implied making a few corrections leading to a few less serious rendering issues (see the known issues below and the Rendering Metadata line above). As a result, blindfolds and other vision restriction devices look a lot better now.
	- fixed : When the vision is restricted, alpha rigged attachments will no longer poke holes through the vision spheres. That was the biggest concern.
	- fixed : When the vision is restricted, particles spawned by the avatar's attachments (such as leashes) are now rendered correctly. That was the other biggest concern.
	- fixed : When zooming up close to a prim, the camera could move out of the vision spheres.
	- fixed : @acceptpermission did not work for avatars whose last name was "Resident".
	- fixed : Selected faces were not rendered with their overlay when blindfolded (this was due to an older fix that did no longer have a purpose).
	- fixed : When changing the draw render limit, visually update all the objects around.
	- fixed : Hide fully transparent, non-phantom prims in-world when the vision is restricted.
	- fixed : Remove the color of all the faces of all the objects in-world when @camtextures is active.


2.09.03.20 (@versionnum = 2090320) by Henri Beauchamp (equivalent to Marine's v2.09.03.00):
- Inclusion of Marine Kelley's changes:
	- fixed : A way to bypass the @editobj restriction.
	- fixed : @recvchatfrom and @recvemotefrom did not take attachments into account.
	- fixed : When teleporting to another region and forced into Mouselook with @camdistmax:0, the mouse pointer would show but we would be stuck in ML nonetheless (thank you Sinha Hynes for the heads-up).
Note: the other fixes in Marine's v2.09.03.00 were already fixed in or are irrelevant to the Cool VL Viewer.


2.09.01.21 (@versionnum = 2090121) by Henri Beauchamp:
- Inclusion of Marine Kelley's git repository changes:
	- fixed : @camavdist did not show silhouettes after a TP.
	- fixed : Keep the camera under control even when it is taken by a script or a seat.
	- fixed : "@detachthis" would lock the whole #RLV folder, just as "@detachthis:", and it would not even be taken into account for items that were just given to the user during this session.


2.09.01.20 (@versionnum = 2090120) by Henri Beauchamp (equivalent to Marine's v2.09.01.00):
- Inclusion of Marine Kelley's changes:
	- fixed... ish : RenderResolutionDivisor was broken, and is still broken, when turning the simulated fog on. It is not fixed yet, but now when using this debug setting while @camdrawXXX commands are active, the blur is set to be very high (256 for now), in order to prevent cheating.
	- fixed : Mouselook was bugged when clicking on Alt, sitting down or teleporting, leaving the avatar stuck.
	- fixed : The simulated fog introduced in 2.9 was not opaque enough, and it got worse when increasing the number of gradients.
	- fixed : The color of the fog was wrong, too much greyish.
	- fixed : @camavdist broke when impostors were turned off (thank you Vanilla Meili for the heads-up).
	- fixed : Request teleport wasn't scrambling the attached message under @sendim.


2.09.00.20 (@versionnum = 2090020) by Henri Beauchamp (equivalent to Marine's v2.09.00.00):
- Inclusion of Marine Kelley's changes:
	- added : @camzoommin and @camzoommax to prevent from zooming in or zooming out too far. This is great in games where the avatar is not supposed to have binoculars integrated directly in the eyes.
	- added : @camdistmin and @camdistmax to force the camera to stay within a range, or at a certain distance from the avatar (or both). Set @camdistmax to 0 to force the view to Mouselook, and @camdistmin > 0 to prevent from going to Mouselook at all. This is great for games where you don't want the players to be able to cam around the sim, like in mazes.
	- added : @camdrawmin and @camdrawmax to simulate fog or even blindness, by obscuring the world around the avatar (not around the camera like windlight settings do). Best used along with @camtextures. The camera is restricted to be inside @camdrawmin * 0.75 on top of it. This is especially fitting for blindfolds.
	- added : @camdrawalphamin and @camdrawalphamax to indicate the closest and farthest opacities of the fog defined by @camdrawmin and @camdrawmax. When @camdrawalphamin is 0 (whicih is the value by default), you are assured that the world beyond @camdrawmax will be behind an opacity of @camdrawalphamax, regardless of the number of spheres rendered (which is decided by the new debug setting "RestrainedLoveCamDistNbGradients", which default value is 10).
	- added : @camdrawcolor to set the color of the fog (default is black)
	- added : @camunlock to prevent the camera from being unlocked from the avatar (it is unlocked when focusing elsewhere, or panning or orbiting the camera). This is great when you don't want the let the user see through walls.
	- added : @camavdist to specify the maximum distance beyond which avatars look like shadows. This is great when blind or partially blind, to let the user know what the other avatars do, but not too clearly.
	- added : @camtextures to make the whole world grey, except the avatars and the water. This is great when used along with @camdrawmin/max, to simulate blindness while still having a "feel" for the world immediately around the avatar. Please note that bump mapping and shininess stay untouched, because the avatar can "feel" whether a surface is smopoth, rough or slippery. (thanks Nicky Perian for the help !)
	- added : @shownametags act exactly like @shownames, except it does not censor the chat with dummy names (but it does hide the radar, the name tags, prevents from doing things to an avatar through the context menu, etc). This is great for games where you have to find your opponents who may be hidden, and who don't want to be betrayed by their name tags.
- Added: a "Forbid camera restrictions" category/check box to the "RestrainedLove Blacklist" floater (this category is included automatically when choosing the "BDSM Role-Player" or "Non-BDSM" user profiles: you will however have to re-instate the profiles in the Preferences/Cool features/RestrainedLove settings so to update your commands blacklist when updating from an older version of the viewer).
- Changed: when any of the @camzoommin, @camzoommax, @camdistmin or @camdistmax restrictions are enforced, the "Disable Camera Constraints" setting is ignored (i.e. all the camera constraints are enforced).


2.08.05.20 (@versionnum = 2080520) by Henri Beauchamp (equivalent to Marine's v2.08.05.10):
- Inclusion of Marine Kelley's change:
    - changed : Do not block llRegionSayTo() on channel 0 even when @recvchat or @recvemote is active, because this kind of chat is very similar to a task IM (an IM from an object to an avatar), only without the built-in delay and valid only in the same sim. Thanks Felis Darwin for the report.


2.08.03.29 (@versionnum = 2080329) by Henri Beauchamp:
- Fixed: an issue with worn clothing items not properly listed as such in inventory after attempting to remove a layer that was locked by RestrainedLove.


2.08.03.28 (@versionnum = 2080328) by Henri Beauchamp:
- Changed: better code to prevent beacons rendering while @edit is in force.


2.08.03.27 (@versionnum = 2080327) by Henri Beauchamp:
- Fixed: potential crash bugs on spurious disconnections or failed teleports while @showloc is in force.


2.08.03.26 (@versionnum = 2080326) by Henri Beauchamp:
- Fixed: properly stack wearables when using @attachover.


2.08.03.25 (@versionnum = 2080325) by Henri Beauchamp:
- Changed: for security reasons, do not allow auto-accept temp objects attachments (via the new llAttachToAvatarTemp() function) when @acceptpermission is in force and when the attach request comes from an object not pertaining to us.


2.08.03.24 (@versionnum = 2080324) by Henri Beauchamp:
- Changed: lifted the restrictions on @adjustheight to deal with tiny and giant mesh-based avatars.


2.08.03.23 (@versionnum = 2080323) by Henri Beauchamp:
- Inclusion of Marine Kelley's changes:
    - changed: when @accepttp is in force, allow force-TPs, even when in busy mode.
    - fixed: Automatic attachment of objects when logging on was not always working, when the folder they are contained into was locked (with @unsharedwear or @attachathis).
- Added: preliminary implementation of the new @relayed command (see: https://bitbucket.org/marinekelley/rlv/issue/61/proposal-new-flag-llike-command-to).
- Changed: When reattaching automatically a kicked object and whenever possible, do so by adding to existing attachments instead of replacing them.


2.08.03.22 (@versionnum = 2080322) by Henri Beauchamp:
- Fixed: a bug that prevented the nostrip protection to work on removal of individual items held inside a folder which name contains "nostrip".


2.08.03.21 (@versionnum = 2080321) by Henri Beauchamp:
- Fixed: a bug that prevented the @remoutfit:wearable_type=force command to properly remove all layers of wearable_type (for Cool VL Viewer v1.26.3 and later).
- Fixed: a bug that prevented Physics wearables to be removed when invoking @remoutfit=force to remove all wearables.
- Changed: added a call to the garbage collector on pressing the Refresh button of the RestrainedLove Restrictions List floater.


2.08.03.20 (@versionnum = 2080320) by Henri Beauchamp:
- Changed: improved the processing of nostrip items (now properly protected against "@behav=force" commands).
- Added: support for multiple clothing layers (for Cool VL Viewer v1.26.3 and later only).


2.08.02.21 (@versionnum = 2080221) by Henri Beauchamp:
- Changed: slightly improved the name hashing for @showname.


2.08.02.20 (@versionnum = 2080220) by Henri Beauchamp (equivalent to Marine's v2.08.02.01):
- Inclusion of Marine Kelley's changes:
    - added: When a script tries to attach non-properly named items, the viewer will attach them anyway, by stacking (i.e. no risk of replacing locked items worn on the same attachment point).
    - fixed: While in @showname=n, replace "Some people" (which is plural, but designates an individual) with "Anonymous One" in the anonymized names list.


2.08.01.21 (@versionnum = 2080121) by Henri Beauchamp:
- Fixed: a crash bug that occurred when someone with a display name of less than 4 characters was chatting while @shownames=n was in force (thanks Kathrine Jansma for the bug report).
- Fixed: a breakage in @getinvworn that was introduced in v2.08.01.20 (thanks Ibrew Meads for the bug report and fix).


2.08.01.20 (@versionnum = 2080120) by Henri Beauchamp (equivalent to Marine's v2.08.01.00):
- Inclusion of Marine Kelley's changes:
    - added: @getblacklist[:partial_name]=2222 to retrieve a comma separated list of blacklisted commands.
    - added: @getblacklist in IM to act like @version.
    - added: @versionnumbl=2222 to retrieve both the version and the blacklist.
    - fixed: @detach=n on a child prim did not always make the whole object nondetachable.
    - fixed: @getstatus needs a new, or user-selectable, separator.
    - fixed: @getinvworn can return string literal "n" instead of two numbers for the this/child status indication.
- Fixed: allow to change the blacklist when RestrainedLove was just enabled but not yet actually activated by a viewer restart.
- Changed: @getcommand now always returns both "behav" and "behav%f" to distinguish between "@behav=n" and "@behav=force". RestrainedLoveExtendedGetcommand debug setting removed as a result.


2.07.04.20 (@versionnum = 2070420) by Henri Beauchamp:
- Inclusion of Marine Kelley's change:
    - fixed: Empty tokens in RLV commands ("@showloc=n,,showinv=n", "@showloc=n,") would raise a "failed command" alert, yet execute the command anyway.
- Added: commands black-listing for role-players, non-BDSM users, etc. Also implemented a black-list floater.
- Added: @getcommand[:partial_name_match]=channel to retreive the list of the available and non black-listed commands which name match partial_name_match (returns all non black-listed commands when partial_name_match is omitted). Also, when the new RestrainedLoveExtendedGetcommand debug setting is set to TRUE, @getcommand returns "behav" for @behav=y/n commands and "behav%f" for @behav=force commands.
- Added: RestrainedLoveUntruncatedEmotes setting to always prevent the truncation of emotes when chat is restricted (equivalent to a permanent @emote=add).
- Bumped the nano version number (20) so that Marine's RLV (hopefully) won't ever collide again with mine (the nano was supposed to be reserved for builds, Marine... not for bugfixes...).
- Some code cleanup.


2.07.03.06 (@versionnum = 2070306) by Henri Beauchamp:
- Inclusion of Marine Kelley's (upcoming) change:
    - added: Make gestures activate/deactivate when wearing/removing items respectively through a script.
- Fixed: @setenv_*i commands now work properly and set the intensity in the same way as the corresponding intensity sliders in the Windlight settings floater.
- Fixed: when open, the Windlight settings and Day Cycle editor floaters are now properly updated each time a @setenv_* command is issued.
- Some code cleanup.


2.07.03.05 (@versionnum = 2070305) by Henri Beauchamp:
- changed: when deafened and RestrainedLoveShowEllipsis is FALSE, fully skip chat lines instead of displaying empty lines.


2.07.03.04 (@versionnum = 2070304) by Henri Beauchamp:
- Inclusion of Marine Kelley's (upcoming) change:
    - changed: @detach may now be issued by script in child primitives and (un)lock the whole object.


2.07.03.03 (@versionnum = 2070303) by Henri Beauchamp (equivalent to Marine's v2.07.03.02):
- Inclusion of Marine Kelley's change:
    - fixed: @getinvworn would sometimes give weird results when a single no-mod item was under a folder which name did not begin with "."


2.07.03.02 (@versionnum = 2070302) by Henri Beauchamp (equivalent to Marine's v2.07.03.01):
- Inclusion of Marine Kelley's changes:
    - changed: Now you can customize the automatic message people get when they IM you and you can't receive IMs. Requires a restart of your viewer.
    - changed: Now you can customize the automatic message people get when you IM them but you can't send IMs. Requires a restart of your viewer.
    - changed: You are given the choice whether you can send and receive OOC chat (chat between double parenthesis: "((...))"). Default is TRUE, and it requires a restart of the viewer.
    - fixed: Restrictions were removed silently (i.e. without a notification to scripts) when garbage-collected from an object that had disappeared.
    - fixed: When trying to teleport someone, we were not getting the automatic response if they were prevented from teleporting.


2.07.02.02 (@versionnum = 2070202) by Henri Beauchamp:
- Fixed: a crash bug, introduced in v2.07.02.00, which occurred when right-clicking immediately on a newly created/accepted/copied/moved object in the inventory.
- Fixed: a crash bug when trying to teleport two or more friends at once.


2.07.02.01 (@versionnum = 2070201) by Henri Beauchamp (equivalent to Marine's v2.07.02.00):
- Inclusion of Marine Kelley's changes:
    - changed: "Dummy names" ("a person", "this individual"...) are now scrambled every few hours when @shownames=n is in force. This allows even a close friend whose dummy name is well known to still be able to surprise you during roleplay. They are not scrambled at every relog though, to avoid confusing the user under crashy conditions.
    - fixed: Copy/pasting items from/to a locked folder (a folder to which a lock or an exception to a lock has been issued). Renaming folders and moving objects from an unshared folder to another one is ok though.
- Fixed: issues with *_sec variants and @permissive.


2.07.00.02 (@versionnum = 2070002) by Henri Beauchamp:
- Change: when "Wear"ing an object do not search for attachment names in that object name anywhere but between an opening and a closing parenthesis; i.e. "Top dress attachment" will not be considered as to be attached at the "Top" HUD position any more, but "My preferred HUD (top - whatever here)" is still properly identified as a Top HUD attachment.


2.07.00.01 (@versionnum = 2070001) by Henri Beauchamp (equivalent to Marine's v2.07.00.00):
- Inclusion of Marine Kelley's changes:
    - added (*): Support for the new body physics in the regular outfit commands: @getoutfit, @remoutfit, @addoutfit, @attachthis, @detachthis...
    - added: @temprun command to prevent from running by double-tapping on an arrow key, this does not prevent from running with Ctrl-R though, one must use @alwaysrun at the same time to prevent from running at all.
    - added: @alwaysrun command to prevent from running by pressing Ctrl-R, this does not prevent from running by double-tapping on an arrow key though, one must use @temprun at the same time to prevent from running at all.
    - fixed: @standtp did not work when sitting on the ground.
- Fixed: the force-sit on ground feature did not check for the @sit=n restriction.

(*) Only in the Cool VL Viewer v1.26.0 and later: Avatar Physics is not implemented in v1.25.0 and former versions.


2.06.00.02 (@versionnum = 2060002) by Henri Beauchamp:
- Changed: made it impossible to change a couple of debug variables from the debug settings floater so to prevent cheating.


2.06.00.01 (@versionnum = 2060001) by Henri Beauchamp (equivalent to Marine's v2.06.00.00):
- Added: a RestrainedLove Restrictions list floater, to be popped up from Advanced -> RestrainedLove.
- Inclusion of Marine Kelley's changes:
    - added: @touchme=add exception to the @touch*=n restrictions, this works only for the oject that issues this restriction, to avoid cheating.
    - added: @startim=n command, to prevent the user from opening an IM session with anybody (the little "trill" sound will still play, though).
    - added: @startim:UUID=add exception to the above restriction.
    - added: @startimto:UUID=n command, same as @startim, but regarding one particular person only.
    - added: Debug setting RestrainedLoveShowEllipsis (default to TRUE) to show or hide the "..." when you are under @recvchat or @recvchatfrom.


2.05.00.03 (@versionnum = 2050003) by Henri Beauchamp:
- Changed: do not any more postpone RestrainedLove commands execution whenever the viewer window is on another virtual desktop or minimized (caused timeout in scripts, especially for @attach/@detach(& Co) commands).


2.05.00.02 (@versionnum = 2050002) by Henri Beauchamp (equivalent to Marine's v2.05.00.01):
- Inclusion of Marine Kelley's changes:
    - added: Notification to scripts on clothes and attachment changes. This allows a script to be notified when the user wears or unwears an outfit, without having to constantly poll for the list of worn items. The notifications are "(worn/unworn/attached/detached) (legally/illegally) (path_to_outfit)"
    - added: "@attachoverorreplace", "@attachthisoverorreplace", "@attachalloverorreplace" and "@attachallthisoverorreplace" commands, which now do what respectively "@attach", "@attachthis", "@attachall" and "@attachallthis" used to do until now, in preparation for a possible change in the future.
    - added: "@setgroup:(name)=force" command to force the user to activate the specified group (they must be a member of course). "none" will deactivate the group tag.
    - added: "@setgroup=n" to prevent the user from switching groups.
    - added: "@getgroup=2222" to obtain the name of the current activated group of the user. Please note that the UUID of the group is not disclosed, only the name, to stay consistent with the LSL API.
    - added: "@touchworld:(uuid)=add" exception to allow to touch an specific object in world even when "@touchworld=n" is active.
    - added: "@touchthis:uuid=n" restriction to prevent from touching one object in particular.
    - added: "@unsharedwear=n" command to prevent the user from wearing anything that is not under #RLV. (*)
    - added: "@unsharedunwear=n" command to prevent the user from unwearing anything that is not under #RLV. (*)
    - added: "@detachthis_except:(folder_child)=add" to add an exception to a "@detachallthis:(folder_parent)=n" restriction, provided that (folder_child) is contained somewhere under (folder_parent) and that "@detachthis_except" has been issued by the same object (this kind of exception does not work between different objects, on purpose). (*)
    - added: "@detachallthis_except:(folder_child)=add", same thing as above but will include all the sub-folders in the exception as well. (*)
    - added: "@attachthis_except:(folder_child)=add" to add an exception to a "@attachallthis:(folder_parent)=n" restriction, provided that (folder_child) is contained somewhere under (folder_parent) and that "@attachthis_except" has been issued by the same object (this kind of exception does not work between different objects, on purpose). (*)
    - added: "@attachallthis_except:(folder_child)=add", same thing as above but will include all the sub-folders in the exception as well. (*)
    - changed: Now "@attachall:=force" (notice the lack of option after the colon) is ignored, to avoid a common mistake that would make the user attach their whole #RLV folder (and given the time it takes, probably disconnect their viewer in the process).
    - fixed: Could remove the jacket layer even when contained into a shared folder and "@detachthis:jacket=n" was issued.
    - fixed: Could change shape even under "@remoutfit:shape=n".


2.04.00.03 (@versionnum = 2040003) by Henri Beauchamp:
- Changed: extended the functionality and syntax of @adjustheight. You can now auto-correct the avatar height for a given animation via @adjustheight:ref_pelvis_to_foot_length_in_meters;scalar[;optional_delta_in_meters]=force with ref_pelvis_to_foot_length_in_meters being the pelvis to foot length for which teh animation is properly leveled with the floor, and scalar the multiplicator to apply to the difference between the said reference length and the current pelvis to foot length to obtain the proper avatar height adjustment offset and keep the anim levelled with the floor. Example, for the well known nadu" kneeling anim: @adjustheight:0.97;1.6=force. Note that @adjustheight:adjustment_in_centimeters=force is still valid for constant offsets (independent from the avatar shape).


2.04.00.02 (@versionnum = 2040002) by Henri Beauchamp:
- Added: a new @adjustheight command to adjust the avatar height so to auto-correct animation height via scripts. The syntax is @adjustheight:adjustment_in_centimeters=force with adjustment_in_centimeters from -100 to 100.


2.04.00.01 (@versionnum = 2040001) by Henri Beauchamp:
- Changed: the version numbering of the RestrainedLove API will now again match Marine's RLV (used to differ because of incoherent version numbers in previous RLV2 versions). The major, minor and micro (here 2.4.0) will exactly match Marine's, while the nano (here 1) will be used to distinguish my implementation from hers.
- Inclusion of Marine Kelley's changes:
    - changed: double-click teleport is now prevented only when the forward control is grabbed (i.e. intercepted by a script and not passed to the agent) and something is locked.
    - fixed: Edit and Build on land were deactivated on right click menu.
    - fixed: a clever cheat to detach something locked (but it would come back after 5 seconds anyway).
    - added: @recvemotefrom.
    - added: @touchall to prevent the avatar from touching anything (including attachments but not HUDs).
    - added: @touchworld to prevent the avatar from touching any object in world (does not apply to attachments and HUDs).
    - added: @touchattach to prevent the avatar from touching any attachment, including theirs (does not apply to HUDs).
    - added: @touchattachself to prevent the avatar from touching their own attachments (does not apply to HUDs).
    - added: @touchattachother to prevent the avatar from touching any attachment, except theirs.


1.25b (@versionnum = 1250100) by Henri Beauchamp:
- Added: filtering of display names for @shownames.
- Changed: RestrainedLoveAllowWear and RestrainedLoveAddReplace are now set to TRUE by default.


1.25a (@versionnum = 1250000) by Henri Beauchamp (equivalent to Marine's v2.3):
- Inclusion of Marine Kelley's changes:
    - added : @detachthis and @detachallthis restrictions to prevent the avatar from removing certain outfits.
    - added : @attachthis and @attachallthis restrictions to prevent the avatar from wearing certain outfits.
    - added : @editobj command to prevent editing and opening one object in particular.
    - added : exception mechanism to the @edit restriction (not to be mistaken with @editobj mentioned above), to allow the avatar to edit or open one object in particular.
    - added : @sendimto, @recvimfrom and @recvchatfrom restrictions to prevent an avatar from sending IMs to, receiving IMs from and hearing chat from an avatar in particular.
- changed (cosmetic): disable the clothing menu items in the inventory context menus (folder and cloth layer context menus) whenever the corresponding actions are restricted (such as disabling "Take Off" or "Wear" when, respectively, taking off a clothing item or wearing it is forbidden) .


1.24d (@versionnum = 1240400) by Henri Beauchamp:
- Fixed: a random crash bug in @detach.


1.24c (@versionnum = 1240300) by Henri Beauchamp (equivalent to Marine's v2.2.0.1):
- Inclusion of Marine Kelley's change:
    - added : No script will be able to remove an item, a piece of clothing or a complete folder if its name contains "nostrip".
Note: The "when in no-script areas, every attachment becomes automatically nondetachable" change included in Marine's v2.2.0.1 is and will *NOT* be implemented in the Cool VL Viewer, since it's an enormous and unacceptable restriction to impose on everyone, just to prevent a rare cheating with badly written RestrainedLove scripts.


1.24b (@versionnum = 1240200) by Henri Beauchamp:
- Fixed: a crash bug (introduced in v1.24a/v2.2) when RL is enabled, the #RLV shared folder absent from your inventory and an item given to and accepted by you.


1.24a (@versionnum = 1240100) by Henri Beauchamp (equivalent to Marine's v2.2):
- Inclusion of Marine Kelley's changes:
    - added : When receiving inventory from an object, the avatar can be made to automatically say a message on a private channel via the @notify command. That way the object knows whether the offer has been accepted, or accepted AND put in the #RLV folder, or declined (including muted).
    - fixed : @attachallthis on an empty attachpoint or clothing layer attached everything !


1.24 (@versionnum = 1240000) by Henri Beauchamp (equivalent to Marine's v2.1.2.2):
This is basically a catchup with Marine's v2.1.2, with full support for multiple attachments.
- Inclusion of Marine Kelley's changes:
    - added : New commands @getpathnew, @attachover, @attachallover, @attachthisover, @attachallthisover, @standtp.
    - changed: When the folder name begins with a "+", @attach:folder=force adds the outfit instead of replacing it.
- Fixed: a bug in @getsitid that resulted in a corrupted UUID when another avatar than yours was sitting within draw distance.


1.23f (@versionnum = 1230006) by Henri Beauchamp:
- Fixed: a failure to save the "TP ok" state on log off whenever no RestrainedLove item was used during the session.
- Added: support for inventory item links.


1.23e (@versionnum = 1230005) by Henri Beauchamp:
- Fixed: fix the "Start Location" combo box manual editing (the viewer failed to log in in the entered sim with v1.23d).


1.23d (@versionnum = 1230004) by Henri Beauchamp:
- Changed: you may now enable the "Add to Outfit" and "Replace Outfit" commands in the context menu for outfit folders, by setting the RestrainedLoveAddReplace advanced setting to TRUE. Be aware however, that just like with RestrainedLoveAllowWear, this may result in failures to reattach locked objects that were accidentally kicked should the grid experience rezzing issues.
- Changed: you may now choose your login location (and use SLURLs from your web browser) as long as you were not TP-restricted when you last logged out.


1.23c (@versionnum = 1230003) by Henri Beauchamp:
- Fixed: when requesting to attach a category (for example with @attachall), give the priority to the category matching exactly the name instead of using the first category encountered with a partial match (for example @attachall:cuffs=force will give the priority to a #RLV sub-folder named "cuffs" over those containing the "cuffs" word among others such as "collar and cuffs").


1.23b (@versionnum = 1230002) by Henri Beauchamp:
- Fixed: a bug in @shownames that crashed the viewer when avatars with chatting HUDs (emoters, for example) were used around the restricted avatar.


1.23a (@versionnum = 1230001) by Henri Beauchamp:
- Inclusion of Marine Kelley's changes:
    - changed : The name of the viewer itself. It is still known as "the RLV", but the meaning of the letters "RLV" change from "Restrained Life Viewer" to "Restrained Love Viewer". This is due to the Third Party Viewer policy that Linden Lab has published on 02/22/2010, forbidding to use the word "Life" or any synonym in the name of a Third Party Viewer such as the RLV.
    - added : due to the name change, a new command appears : @versionnew, which returns "RestrainedLove viewer v1.23.0 (SL 1.23.5)". The old @version command still does the same as before, i.e. return "RestrainedLife viewer v1.23.0 (SL 1.23.5)", but you are encouraged to not use this command anymore in new scripts, and to do your best to hide the name "RestrainedLife" from the user. Use "RestrainedLove" instead, or better, "RLV".
- Changed: commands taking a channel number to send their reply to now accept negative channels (handy to prevent cheating during RestrainedLove detection, e.g. @version=-1 prevents the user to cheat since they can't chat on a negative channel and spoof a RestrainedLove viewer reply on it). Note however that the length of the message returned by the commands on negative channels cannot be greater than 255 characters (instead of 1023 for positive channel numbers), so a negative channel number is not a good choice for commands that could return long strings (such as @getstatusall or @getinv, for example).


1.22h (@versionnum = 1220106) by Henri Beauchamp:
- Added: support for the new Alpha and Tattoo wearable (in @getoufit, @remoutfit, etc). Note that for compatibility reasons, the Alpha and Tattoo flags appear (in this order) after the shape flag in the string returned by @getoutfit=<channel>.
- Changed: The "Hair" body part cannot anymore be removed with @remoutfit. This is because baked hair is now a requirement in v2.0 and later viewers, and an avatar without hair may also result in an unrezzed avatar in v1.23 anyway. If you want to hide the hair bodypart, you can use the new Alpha wearable.


1.22g (@versionnum = 1220105) by Henri Beauchamp:
- Fixed: @redirchat does not truncate emotes any more, like originally intended and documented in the API.


1.22f (@versionnum = 1220104) by Henri Beauchamp:
- Changed: code cleanup.
- Changed: reworked the auto-reattachment feature to make it more reliable and to avoid state loss in reattached items and allow retries on reattach failures. New RestrainedLoveReattachDelay advanced setting implemented.


1.22e (@versionnum = 1220103) by Henri Beauchamp:
- Changed: Speed optimization relative to the RestrainedLoveDebug flag.


1.22d (@versionnum = 1220102) by Henri Beauchamp:
- Added: a RestrainedLove sub-menu in the Advanced menu of the viewer (or Client menu for v1.19.0.5), allowing to easily toggle the advanced RestrainedLove settings.
- Fixed: a bug introduced in Marine's v1.22 and that made the "Empty Lost And Found" item vanish from the "Lost And Found" folder context menu.


1.22c (@versionnum = 1220101) by Henri Beauchamp (equivalent to Marine's v1.22.1):
- Inclusion of Marine Kelley's change:
    - changed (*): The user is now able to focus their camera on objects even through a HUD (except when in Mouselook). This is handy for people who like to spend time bound and "blocked" (meaning their clicks are intercepted by a huge HUD prim across their screen), but who dislike being unable to focus.
(*) does not (yet) apply to viewer v1.19.0.5.


1.22b (@versionnum = 1220002) by Henri Beauchamp:
- Fixed: crash bugs that could occur while the avatar is rezzing (during logins or after TPs), especially when the viewer window is minimized or not displayed (i.e. on another workspace than the current one).


1.22a (@versionnum = 1220001) by Henri Beauchamp (equivalent to Marine's v1.22):
- Inclusion of Marine Kelley's changes:
    - fixed : inventory context menu would not refresh properly after everything is unlocked.
    - added : @addattach and @remattach commands, to do what the old @detach:point=n command did (this command keeps being valid, and must be seen as an alias to @addattach and @remattach used at the same time). Thanks to all who gave their opinions and allowed a little brainstorm on this one !
    - added : @viewscript and @viewtexture, which work exactly like @viewnote, on scripts and textures (and snapshots) respectively. Thank you Yar Telling for the hint !
- Adaptation of the new code to v1.19.0.5.
- Minor cleanup and optimizations.


1.21a (@versionnum = 1210101) by Henri Beauchamp (equivalent to Marine's v1.21.1):
- Inclusion of Marine Kelley's changes from v1.21:
    - fixed : A clever way to cheat around @shownames (thank you Talisha Allen).
    - changed (1): Reinstated "Wear" on the contextual menu even when something is locked and no attach point is contained in the name of the item. This holds the risk of kicking a locked object off, but it will be reattached automatically after 5 seconds anyway. Even "Add To Outfit" and "Take Off Items" work. This was a MUCH awaited feature !
    - changed : Added support for reattaching several objects at the same time. Objects will be reattached at 1 second interval.
    - added : @defaultwear restriction. When this restriction is set, the "Wear" command will work like it did before this version, i.e. disappear if something is locked and no attach point information is contained within the name. This is for subs who tend to abuse the Wear menu and kicking off locked objects a little too often.
    - added : @versionnum command to retrieve the version number directly, instead of having parse the "RestrainedLove viewer v1.20.2 (1.23.4)" string. Here it will return "1210000".
    - added : @permissive command that tells the viewer that any exception to @sendim, @recvim, @recvchat, @tplure, @recvemote and @sendchannel MUST come from the object that issued it or will be ignored (without this command, any object can set an exception to the restrictions issued by any other object).
    - added : @sendim_sec, @recvim_sec, @recvchat_sec, @tplure_sec, @recvemote_sec and @sendchannel_sec to do the same as @permissive, but one restriction at a time (i.e. exceptions to @sendchannel from other objects won't be ignored if @sendim_sec is set).
- Inclusion of Marine Kelley's changes from v1.21.1:
    - fixed : locking attachment points was not working anymore.
    - fixed : massive reattach after an "Add To Outfit" command could fail with a "pending attachment" kind of error message in laggy areas (thank you Henri Beauchamp).
    - changed (2): "Add To Outfit", "Take Off Items" and "Replace Outfit" menu items will be hidden if something is locked (object or clothing) inside the folder you have selected, or any of the folders it contains, recursively.
    - known issue : These menu items will be hidden even if there are only clothes in the folder and none of them is locked, but another piece of clothing is locked specifically (for instance, you are trying to unwear pants, but the shirt is locked). This is because of a limitation in the code and will be corrected in a future version.
- Work around for (1): Allowing the "Wear" command in inventory context menus makes it possible to have locked attachments kicked. Theoretically, the RestrainedLove code is able to reattach these kicked locked attachments. Alas, and because of the low reliability and extremely variable delays encountered when asset server operations are involved, the reattachment may fail. This is why I implemented a new debug setting ("RestrainedLoveAllowWear"): when FALSE (which is the default), the "Wear" command is unavailable and RestrainedLove behaves like in v1.20 and previous versions. When TRUE, the "Wear" command availability is governed by the @defaultwear command (i.e. "Wear" is available by default), like in Marine's v1.21.
- Work around for (2): Because of automatic reattachment failures, "Add To Outfit", "Take Off Items" and "Replace Outfit" are for now disabled (when at least one worn attachment is locked) for all folders and not just for folders containing locked items.


1.20c by Henri Beauchamp (equivalent to Marine's v1.20.2):
- Inclusion of Marine Kelley's changes:
    - fixed : a nasty crash when reattaching a locked object, introduced in 1.20. Thanks to all who helped tracking this down.
    - fixed : a workaround @tplm that was supposedly fixed in 1.20.
- Code clean up and optimizations.


1.20b by Henri Beauchamp (equivalent to Marine's v1.20.1):
- Inclusion of Marine Kelley's changes:
    - fixed : crash to desktop when hearing chat from an unrezzed avatar while under @shownames (bug introduced in 1.20).
    - fixed : crash to desktop when forcing an object to be detached then locking its attach point right away, which would trigger an infinite loop (introduced in 1.20).
    - fixed : a cheat around @shownames (thanks Jolene Tatham). (*)
(*) Did not affect the Cool SL Viewer v1.19.0.5.


1.20a by Henri Beauchamp (equivalent to Marine's v1.20):
- Inclusion of Marine Kelley's changes:
    - added : @notify command to let scripts be notified when a particular restriction (or just any restriction) is issued or lifted by an object. It does not disclose the object itself, just the fact a restriction has changed. (thank you Corvan Nansen for the idea)
    - added : @detach:<attach_point> command to lock a particular attachment point. When using this command, any object worn there is locked on, even if it is not even scripted, and no other object can kick it off. If the attachment point is empty, this command will lock it empty, even if another object is attached to it with llAttachToAvatar(). (thank you Chorazin Allen for the idea)
    - changed : improved the attachment point calculation in the names of inventory items. Now it looks from right to left (to be consistent with how the RLV renames items when worn), and will select the candidates with the longest names first. In other words, it makes the RLV ready if the number of attachment points is increased (like adding "chest (2)" and the like).
    - changed : hide custom text in friendship offers when unable to receive IMs.
    - fixed : HUDs and unrezzed objects and avatars were immune to @recvchat (they could always be heard). (thank you Jennifer Ida for reporting this)
- Code clean up and optimizations.


1.19b by Henri Beauchamp:
- Never redirect (@redirchar, @rediremote) Out Of Character chat (text starting and ending with double parenthesis): the players must be able to safeword or voice a personal problem/concern.


1.19a by Henri Beauchamp (equivalent to Marine's v1.19):
- Inclusion of Marine Kelley's changes:
    - added : now allows to hide the hovertext floating over one prim in particular (not necessarily the one that issues the command), or all the hovertexts, or only the ones on the HUD, or only the ones in-world. Thank you Lyllani Bellic for the idea.
    - added : @rediremote to redirect emotes to private channels like @redirchat does. Now that one was a popular request !
    - added : @recvemote to prevent hearing emotes like @recvchat prevents hearing chat, also with exceptions. Not as popular but as handy !
    - changed : Now the hovertexts are refreshed immediately when issuing some of the RLV commands (sounds easy, but it was a pain in the **** to implement).
    - fixed : @acceptpermission was broken in 1.18. Partly my fault, sorry.
    - fixed : chat messages in history were showing a weird dot (".") on a single line when prevented from hearing chat. An old bug.
    - fixed : @getpath didn't work in a child prim. Thank you Henri Beauchamp for the tip.
    - fixed : "@attach:main=force" unified with ".Backup (main)" !


1.18a by Henri Beauchamp (equivalent to Marine's v1.18):
- Inclusion of Marine Kelley's changes:
    - changed : now showing (PG), (Mature) or (Adult) even when the location is hidden.
    - fixed : the world map and minimap buttons were not turning themselves off properly when @showloc was issued while they were activated.
    - fixed : now unable to chat on CHANNEL_DEBUG while under @sendchat (thank you Sophia Barrett).
    - fixed : "so and so gave you..." now hides the name while under @shownames.


1.17b by Henri Beauchamp:
- changed: removed the "llOwnerSay() beginning with two spaces not displayed to RL users" feature (introduced by Marine in v1.16), since it breaks existing scripted items that have nothing to do with RestrainedLove...


1.17a by Henri Beauchamp:
- fixed : allow again non-prim hair to be removed via @remoutfit.
- fixed : a crash bug when the #RLV folder is missing and llGiveInventoryList() is used to give a sub-folder to #RLV.
- changed: do not prevent "Go To" or DoubleClickAutoPilot when some device is locked (which does not make sense), but only when llTakeControl() was used to take control on CONTROL_FWD (thus preventing to override any speed limitation or movement restriction).
- Inclusion of Marine Kelley's changes (from v1.17):
    - fixed : visual clues about the map and minimap were a bit... clueless at times.
    - changed : don't go to third view while in Mouselook and switching back to SL from another application. Doesn't work if the window was minimized or hidden (on MacOS X for example).
    - changed : don't allow partial matches on folders prefixes with a "~" character anymore, to avoid taking precedence over the "regular" folders. Thank you Mo Noel for the heads up.
    - changed : now PERMISSION_TRIGGER_ANIMATION is also granted when sitting while @acceptpermission is active, even if the object we are sitting on does not actually contain the animation. Useful for rezzable pose-balls.
    - added : @setrot:angle=force. This command allows you to make the avatar turn to a direction, in radians from the north. This is not possible through a LSL function call so here it is. Be aware that this command is not more precise than the llGetRot() LSL call (for instance the avatar won't rotate if the rotation is less than a few degrees), but it is better than nothing. It is much more precise while in Mouselook, and does not do anything while sitting.


1.16g by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from v1.16.2):
    - fixed : RLV rarely forgets to activate restrictions on relog in particularly laggy areas. This was due to the viewer calling its garbage collector too early, hence clearing restrictions while the restraints had not rezzed yet. The solution I used to fix that is very simple (don't call the garbage collector before a few minutes on startup), but that should do the trick.
    - fixed : RLV checking whether you had locked HUDs at every frame. Now cached to improve performance.
    - fixed : @shownames was showing "An unknown person" for about 20% of the people around. Well silly me. Using a signed char (-128 > +127) for a positive hash (0 > 255) was not a brillant idea. (*)
    - fixed : minor memory leak in RLInterface::forceAttach. (*)
    - fixed : @unsit was not always unsitting you. This bug has been there since the beginning and was very annoying. (*)
    - fixed : a few clever partial workarounds for some restrictions... (*)
    - fixed : @chatshout and @chatwhisper were also changing the range of the automatic viewer responses.
    - fixed : @remoutfit:xxx=force also allowed to remove bodyparts (but it was not visible on the screen).
    - fixed : @acceptpermission was too... permissive.
    - removed : @denypermission is now deprecated. It was there to prevent a script from kicking a locked object with a llRequestPermissions(PERMISSION_ATTACH) followed by a llAttachToAvatar() but since locked objects now automatically reattach themselves, this restriction makes no sense anymore and is only annoying people who do want their HUDs to attach automatically.
    - changed : removed the throttle on permissions concerned by @acceptpermission (since we don't see the dialogs anyway). Thank you Mo Noel for reporting this.
    - added : Visual clues on the lower tool-bar : Map, Minimap, Build and Inventory now update themselves according to their respective restrictions. (*) (1)
    - added : @detachme=force. This "simple" command just makes an attachment detach itself and only itself if not locked. There was a need (even if a script could do it with a llDetachFromAvatar call, after granting permission) because the script needs to make sure the restrictions are cleared before detaching, by issuing a @clear,detachme=force list of commands. Before that, you had to call "@clear", wait a little, then detach the item and pray that it would not reattach itself after 5 seconds.
    - added : @sit=n. This simple restriction has been added to reinforce the security of most cages, in which the prisoner does not have any opportunity to sit anyway. Thank you Chorazin Allen for the idea.
- changed : Inventory offers to #RLV is now enabled by default (since it is now officially supported in Marine's v1.16.2). It can be disabled by setting RestrainedLoveForbidGiveToRLV to TRUE.
- bugfix: fixed a bug in (1) (see above) which prevented the tool-bar "Build" button to get properly updated when RestrainedLove is disabled.


1.16f by Henri Beauchamp:
- changed: @putinv has been removed and only the #RLV/~folder redirection has been kept (meaning a standard Keep/Discard/Mute dialog is always presented to the user). This inventory redirection is only active when the RestrainedLoveAllowGiveToRLV environment variable is set to TRUE.


1.16e by Henri Beauchamp:
- fixed: removed the (undocumented) limitation that made it impossible to force-sit an avatar under @fartouch=n restriction (bug introduced in v1.16), as it breaks existing contents and is very disputable.
- changed: @putinv now only accepts #RLV/~folder (the tilde prefix is mandatory) for items given via llGiveInventoryList(), and cannot be used by an item held into such a #RLV/~folder.


1.16d by Henri Beauchamp:
- changed: @putinv is now disabled by default and can be enabled by setting the RestrainedLoveAllowPutInv debug setting to TRUE.
- changed: when @putinv is in force, do not hide any more in the chat log the name of the folder given via llGiveInventoryList(id, "#RLV/folder", list_of_stuff).


1.16c by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from v1.16.1):
    - fixed : removed a way to force an avatar to talk on channel 0. Thanks Maike Short
    - fixed : @getinvworn would return wrong results when a modifiable object was contained inside a folder named ".(right hand)", for instance. Thanks Satomi Ahn
    - fixed : the viewer would not automatically answer RLV queries when minimized or hidden on MacOS X
    - fixed : a clever cheat around @showloc
    - fixed : a clever cheat around undetachable HUDs.
    - fixed : @getpath:shirt would return the path to the shirt item even if it was not shared.
    - changed : "dummy names" begin with a capital again. It was a try, but it didn't look good.
    - changed : first and last names only won't be hidden anymore, only full names. Thanks and sorry Mo "My short name messes my dialog boxes up !" Noel
    - added (1): when a locked object is detached anyway, by any means, it is automatically reattached 5 seconds later (not sooner, to avoid rollbacks), and in the meantime every RLV commands are ignored to avoid infinite loops.
    (1) This feature does not work with llDetachFromAavatar(). See the work around below.
- fixed: crash bug while under @fartouch restriction and CTRL-selecting a prim. Fix by Kitty Barnett.
- fixed: a glitch allowing to circumvent the @fartouch restriction. Fix by Kitty Barnett.
- fixed: a couple of bugs in v1.16.1 changes above.
- work around: when a locked object is detached and fails to be reattached (see (1) above), do not block the RLV commands after the reattach delay has elapsed.
- changed: changes to RestrainedLoveAllowSetEnv now only take effect after a viewer restart, so to be consistent with Marine's v1.16.1 RLV behavior. Note that Marine's RLV v1.16.1 still does not handle correctly this flag (@setenv=n is still possible with Marine's code when RestrainedLoveAllowSetEnv is TRUE, which is a bug).
- added: new @putinv:avatar_id=add/rem command, allowing avatar_id to issue (via a relay if avatar_id != victim_id) llGiveInventoryList(victim_id, "#RLV/subfolder", list_of stuff), so that objects ("list_of_stuff") is added to the #RLV folder of victim_id, into a new "subfolder". Adapted from a proposal and patch by Saunuk Flatley.


1.16b by Henri Beauchamp:
- Fixed the @getdebug_* and @setdebug_* bugs.
- Removed the llGetAgentLanguage() identification method as it is going to be removed from Marine's RL 1.16.1 and breaks some existing contents.


1.16a by Henri Beauchamp (equivalent to Marine's v1.16):
- Inclusion of Marine Kelley's changes:
    - fixed (1) : improved touch. Now the viewer compares with the actual point the user clicks on, instead of the center of the root prim.
    - fixed : could bypass the @sittp restriction under special conditions (depending on the sit target).
    - fixed : improved speed (muchly) while under many restrictions, by caching them.
    - added : @accepttp restriction has been extended to accept TP offers from anyone when no parameter is given (before a parameter was mandatory).
    - changed : now even hovertexts and dialog boxes are "censored" when prevented from seeing names or location. This will make it difficult to cheat with a radar now !
    - changed : friends won't show in yellow in the minimap under a show names restriction (it is a new feature in the SL viewer v1.22.*).
    - added : @setdebug and @getdebug, working exactly like @setenv and @getenv, but for debug settings. For the moment only AvatarSex (to get/set gender) and RenderResolutionDivisor (to make the screen blurry) are accepted. All the other debug settings are ignored.
    - added : @redirchat to redirect chat spoken on channel 0 to any other private channel, thusly prevent the user from speaking on channel) 0 at all (not even a "..."). This does not apply to emotes, and if several @redirchat restrictions are issued, all of them are taken into account (i.e. chat will be dispatched over several channels at once). This was a very popular request !
    - changed : @getstatus now prepends a slash ("/") before the returned message to prevent from griefing. This does not confuse llParseString2List() calls in a script, but does confuse llParseStringKeppNulls().
    - changed : the case is now ignored when names are censored.
    - added : @getpath to get the path from #RLV to the object, or to the object which occupies the attachment point given as parameter, or to the piece of clothing given as parameter. The object or clothig must be shared, otherwise it returns nothing.
    - added : @attachthis, @attachallthis, @detachthis, @detachallthis commands, which are shortcuts to a @getpath call followed by an @attach, @attachall, @detach or @detachall command respectively. Very handy to manage outfits without breaking the privacy of the user's inventory !
    - added : if an owner message (llOwnerSay) begins with two spaces, it will be hidden to the user. Like a remark or a comment. Regular viewer users will see it normally, of course.
    - changed : added many more "dummy names" for the @shownames restriction. There are 28 of them now. The hash function should be better as well, it was choosing the name based on the length of the name of the avatar before, which could end up in many times the same name around, confusing the user.
    - fixed : don't prevent teleporting when unable to unsit but not currently sitting.
    - added (1): when a script calls llGetAgentLanguage() on a RLV user, the result will be "RestrainedLove Viewer v1....", exactly like a @version call. The user cannot prevent the viewer from returning this, no matter which language they are using and whether they have checked the "make language public" checkbox or not. This is experimental, if it bothers too many people I will remove it in the next version, but not later. In other words, if it is still there in the next version, it will stay there.
    - added : @acceptpermission to automatically accept permissions to attach and to take controls, @denypermission to automatically deny those permissions (the latter takes precedence over the former, of course).
    - fixed : a small cheat around @sendchat. Thank you Vanilla Meili !
(1) This does not apply to v1.19.0.5 based viewers since it relies on code present in v1.21 or later viewers.


1.15c by Henri Beauchamp (equivalent to Marine's v1.15.2):
- Inclusion of Marine Kelley's changes:
    - fixed : invisible folders (starting with ".") were taken into account in the @attachall command
    - fixed : items that are neither objects nor pieces of clothing were taking into account in the @getinvwon command
    - fixed : skin and hair did not register in the @getinvworn command
    - fixed : viewer was freezing when using @getinvworn while RLV debug is active. Thank you Mastaminder McDonnell !
    - fixed : @getinvworn was seeing every item contained directly under #RLV, but did not allow the user to attach nor detach them. As #RLV is not an outfit, now @getinvworn ignores them
    - changed : now when the user is unable to edit things, they are also unable to see any beacon, including invisible objects
- fixed: because of a typo, RestrainedLoveAllowSetEnv was not working properly. It has been replaced by RestrainedLoveNoSetEnv (to ignore both @setenv and @setenv_* commands when set to TRUE) and is now working properly.


1.15b by Henri Beauchamp (equivalent to Marine's v1.15.1):
- Inclusion of Marine Kelley's changes:
    - fixed : an empty shared folder would, in certain cases, mess the information provided by @getinvworn (saying nothing to wear while there are items there). Thank you Julia Banshee !
    - fixed : a piece of clothing alone in a folder, and no-mod would be treated as a no-mod object
    - fixed : a no-mod object which name contains the name of an attachment point would use it even if it was contained inside a folder which name contains another attachment point. Thank you Charon Carfagno !
- added: implemented the RestrainedLoveAllowSetEnv flag (TRUE by default) to allow or forbid (when set to FALSE) environment (day time, Windlight) changes via @setenv_* commands.
- added: implemented @getenv_daytime for v1.19.0.5 viewers.


1.15a by Henri Beauchamp (equivalent to Marine's v1.15):
- Inclusion of Marine Kelley's changes:
    - fixed : order of HUD attachments would make a "top" HUD attach to "top left" and a "bottom" HUD attach to "bottom right", messing them.
    - fixed : detaching a shared folder through a script would also detach one item from a sub-folder regardless of its perms (it was done so primarily for no-mod items before instating sub-folder sharing). Thank you Mastaminder McDonnell for the bug report !
    - fixed : prevent grabbing/spinning when unable to edit things as well.
    - fixed : an old loophole that surfaced again. Thanks TNT74 Pennell and Giri Gritzi !
    - fixed : @setenv_densitymultiplier and @setenv_distancemultiplier were not accurate
    - changed : when trying to attach/detach a folder through a script, whatever is after a pipe ("|") in the name is ignored (pipe included). This is for convenience after using the @getinvworn commmand.
    - changed : now unable to teleport when unable to unsit (that needed unnecessary additional restrictions such as a leash that was not really needed)
    - added : @accepttp command to force the sub to accept a teleport from someone (not necessarily a friend). This does not deprecate @tpto which teleports to an arbitraty location, while @accepttp teleports to an avatar. To the sub it will look like they have been teleported by a Linden (no confirmation box, no Cancel button).
    - added : @getinvworn command to which folders are containing worn items. It roughly works like @getinv, with more information... but it's quite uneasy to explain here, please refer to the API.
    - added : @chatwhisper, @chatnormal and @chatshout commands, which prevent from whispering, chatting normally or shouting respectively. It is different from @sendchat because they do not discard chat messages, they just transform a whisper to normal, normal to whisper, and shout to normal respectively. If all of these restrictions are active, the avatar can only whisper. This kind of command is useful in prisons where some prisoners like to shout all the time.
    - added : @getstatusall command that acts exactly like a @getstatus, but will list all the restrictions the avatar is currently under, without of course disclosing which object issued which restriction.
    - added : @attachall and @detachall commands, which work exactly like @attach (a folder) and @detach (a folder), but recursively. This means it will attach/detach whatever is inside a folder, and in its children as well.
    - added : @getenv_...=nnnn command to get the current Windlight parameters. Works exactly like @setenv_...=force, with the same names. (1)
- Fixed a problem with wrong animation being played whenever @chatwhisper, @chatnormal or @chatshout are in force.
- Fixed a bug that would have crept up when RestrainedLove is disabled in the viewer (which is impossible in Marine's viewer).
(1) This new feature cannot be backported to v1.19 viewers, since they don't implement the Windlight renderer. "@getenv_*" commands are therefore not implemented for v1.19 viewers and are ignored.


1.14c by Henri Beauchamp (equivalent to Marine's v1.14.2):
- Inclusion of Marine Kelley's changes:
    - fixed : crashing when editing something beyond 1.5m, while being prevented from touching things over that distance.
    - fixed : very odd behavior when clicking on something while being prevented from touching things more than 1.5m away.


1.14b by Henri Beauchamp (equivalent to Marine's v1.14.1):
- Inclusion of Marine Kelley's changes:
    - fixed : a bug that should have been fixed in 1.14, but was not tested. And when it's not tested, it's not working, says murphy's law. My bad.
    - fixed : two Windlight control commands were not implemented in 1.14 (@setenv_sunglowfocus and @setenv_sunglowsize). Does not impact v1.19 viewers.
    - fixed : can't detach an attachment when unable to edit (that's a bug introduced in 1.14).


1.14a by Henri Beauchamp (equivalent to Marine's v1.14):
- Inclusion of Marine Kelley's changes:
    - added : WindLight control, so land owners (for instance) can control the way the visitors see their place, provided they use a RLV and a relay. This is a powerful feature meant for scripters, but not really BDSM-related. (1)
    - fixed : a few small bugs, thanks Laylaa Magic and Crystals Galicia !
    - changed : if the sub is prevented from seeing location and their owner is sending a TP (2) offer but forgot to change the text (hence having "Join me in..."), the text will be hidden.
- fixed: a crash in v1.21 viewers when force-sat on login and the object is not yet rezzed.
- fixed: a bug (typo) in Marine's change (2) above.
- changed: the RestrainedLoveDebug flag now also toggles the log messages in mkrlinterface.cpp.
(1) This new feature cannot be fully backported to v1.19 viewers, since they don't implement the Windlight renderer. Only the "@setenv_daytime" setting is supported, the others are ignored.


1.13a by Henri Beauchamp (equivalent to Marine's v1.13.1):
- Inclusion of Marine Kelley's changes:
    - added : new command @findfolder:<part1&&part2&&...&&partN>=2222 to find a particular folder (it returns the full path of the first occurrence, in depth first).
    - changed : the viewer can now handle sub-folders under the shared root (see API). Current scripts that are used to force wearing/unwearing shared outfits need to be modified in order to use that feature, though, otherwise they can only use the first level of folders.
    - changed : shared folders can now be "disabled", which means they won't be seen by the viewer when forcing to attach, detach and getting a list. You can disable by adding a dot (".") at the beginning of the name of the folder.
    - changed : now no-mod items see their parent folder being renamed differently : it becomes ".(<attachpointname>)" instead of "<name> (attachpointname)". That way it won't be seen by the viewer anymore when getting the list (see above), and there is no risk of getting a comma (",") in their name anymore. Of course they still attach their no-mod contents like before.


1.12f by Henri Beauchamp (equivalent to Marine's v1.12.5):
- Inclusion of Marine Kelley's fix:
    - fixed : crash (the viewer hangs, it doesn't crash to desktop) under certain circumstances while prevented from seeing the names of people around.


1.12e by Henri Beauchamp (equivalent to Marine's v1.12.4):
- Inclusion of Marine Kelley's fixes and new features:
    - added : command "@getsitid=nnnn" to allow a script to know the UUID of the object we're sitting on. Useful only for scripters and not really a BDSM feature per se. But the viewer could get that information whereas the scripts couldn't, so here it is. Note : although it is a new feature, it is not important enough to justify changing the Minor Version of the viewer (1.13)
    - added : the location and names are now hidden on the Abuse Report window when prevented from seeing location and names respectively, BUT the Abuse Report will be valid nonetheless (ie the Lindens will be able to read it clearly but not the sub)
    - added : now unable to change the Busy automatic response when unable to send IMs. That can be used to set a humiliating message before preventing the sub from sending IMs, for instance *grins* (thanks Eggzist Boccaccio and Neelah Sivocci)
    - added : owner messages are now hiding the region and parcel name when prevented from seeing the location (was done only on object IMs before)
    - added : the URL to the Objects Sharing Tutorial on my blog is added to this notecard in the Shared Folders section above.
    - changed : now unable to drag-select when unable to touch far objects (since you can't Edit-click on far objects either)
    - changed : now unable to shift-drag an object in-world when unable to rez.
    - changed : reinstate Attach To on the pie menu even when something is locked on you (of course trying to attach on an attachment point occupied by a locked item will silently fail). This allows a sub to carry things even when her inventory is locked away.
    - changed : now the names are not clickable anymore while prevented from seeing names.
    - changed : now the Active Speakers window showing who talked recently (both on chat and on voice) is hidden when prevented from seeing names.
    - changed : now the names and locations are "censored" on any message except avatar chat when unable to see names and location respectively. This will defeat the usual radars (known issue : when someone is detected by a radar and not rezzed on the viewer yet, it won't be hidden)
    - changed : now unable to drag things from the texture picker when unable to open inventory.
    - changed : now unable to see scripted beacons when prevented from editing.
    - changed : the viewer won't answer to @getstatus, @getoutfit etc RLV commands on channel 0. It was a way to mimic someone talking.
    - changed : the viewer now *shouts* the automatic responses to @getstatus, @getoutfit etc RLV commands instead of just "saying" them.
    - fixed : no-mod items sometimes wouldn't load automatically in the shared folders (thanks Diablo Payne)
    - fixed (again) : now unable to send chat through emotes. It was already working before, then got broken for some reason (thanks Peyote Short)
    - fixed : crash when giving empty coordinates to @tpto.
- Allow legit emotes (emotes without said text, and possibly truncated in length) in gestures when sendchat=n.


1.12d by Henri Beauchamp (equivalent to Marine's v1.12.3):
- Inclusion of Marine Kelley's fixes and new features:
    - fixed : crash when saving a script in an object that is out of range
    - fixed : double name in the IM panel when prevented from receiving IMs (but it introduces another bug : no name in the floating chat in this case)
    - fixed : ironing out a way to cheat around @shownames (not an easy cheat this time)
    - changed : owner of the land cannot fly even with admin options on
    - changed : cannot change the inventory of an object we're sitting on when prevented from unsitting (thanks Mo Noel)
    - changed (*): little particle twirls around an owned object do not show anymore (that brings some nice applications such as periodically checking the outfit of a sub)
(*) This feature has been implemented differently than in Marine's patch, so that the particles still appear for llOwnerSay() as long as it does not deal with RestrainedLove commands (as this is quite useful to spot an object saying something to you).


1.12c by Henri Beauchamp (equivalent to Marine's v1.12.2):
- Inclusion of Marine Kelley's fixes and new features:
    - added : new @fly command.
    - added : the ability to share no-mod items, and also to Wear them when something is locked, provided the folder that contains them is properly named. See above. That one was long planned and is finally working !
    - fixed : removed the ability to use the pie menu on avatars when names are hidden. That means no direct interaction, but you can of course still reach people through Search etc. (thanks JiaDragon Allen)
    - fixed : removed the ability to see the region name while it was hidden through an easy trick. (thanks Nilla Hax)
    - fixed : snapshots would not force the HUDs to show if the checkbox was not checked first (but it would stay on afterwards). Now it is forced on whenever a HUD is locked.
    - changed : separated the shownames restriction and the showloc restriction, to have one @shownames command.
    - changed : added a couple more "dummy names" when the names are hidden.


1.12b by Henri Beauchamp (equivalent to Marine's v1.12.1):
- Inclusion of Marine Kelley's fixes and new features (but one):
    - fixed : a couple of ways to cheat around location hiding.
    - changed : when unable to see the current location, the names are hidden on the screen, on the tooltips, in the chat, in the edit window, and profiles cannot be opened directly (they still can in Search of course).
    - changed : now unable to teleport a friend when unable to see the location.
    - changed : improved touching far objects, it's more consistent now.
Note: the feature "now unable to fly when unable to teleport" was not implemented in the Cool SL Viewer, because it breaks the leashes of the collars (it becomes impossible for the sub to fly with their dom while leashed). I suggested to Marine to implement a new "@fly=n" command to take care of the fly restriction.


1.12a by Henri Beauchamp (equivalent to Marine's v1.12):
- Inclusion of Marine Kelley's fixes and new features:
    - added : Force Teleport feature. The sub can be forced to teleport to any location, without asking for permission and without providing a Cancel button. Known issue : if the destination land has a telehub or a landing point, the sub will teleport there.
    - added : No Show Location feature. The sub can be prevented from seeing in which region and parcel they currently are, teleported from, creating a landmark in, trying to buy etc. World Map is hidden too. This is still experimental, I have gone a long way to hide this information everywhere (even "censoring" system and object messages to hide the location), but there might still be places I have missed. I will not, however, "censor" Owner messages so radars can still overcome this restriction.
    - changed : cannot Return/Delete/Take Copy/Unlink objects we are sitting on when we are prevented from unsitting or sit-tping.
    - changed : cannot Open or Edit anything that is further than 1.5 m away when we are prevented from touching far objects.
    - changed : cannot Open objects when we are unable to Edit. It might change later though.
    - fixed : long item names would be renamed improperly by the viewer, now they are truncated before being renamed.
    - fixed : @detach:china=force would detach whatever you wear on Chin instead of whatever is contained into the China shared folder. Thanks Mo Noel for seeing this.

1.11h by Henri Beauchamp:
- Fixed a bug introduced by Marine in her 1.11.5.1 patch (spurious and excessive log lines issued when HUDs are attached, which lead to a slow down and huge log files).


1.11g by Henri Beauchamp (equivalent to Marine's v1.11.5.1):
- Inclusion of Marine Kelley's fixes:
    - fixed : a weird crash when holding Alt while in Mouselook.
    - changed : Hide HUDs in snapshots is now prevented only when a HUD is locked, not when just any attachment is locked.
    - changed : Zoom out on the HUDs is now restricted only when a HUD is locked, not when just any attachment is locked.


1.11f by Henri Beauchamp (equivalent to Marine's v1.11.5):
- Inclusion of Marine Kelley's fixes:
    - changed : cannot change the group tag if you are unable to send IMs anymore.
    - changed : cannot Take and Return objects if you are unable to Rez things.
    - changed : could not Delete objects when unable to Edit, now it is linked to Rez instead, since Rez is dedicated to build/remove objects while Edit is dedicated to modify existing content.
    - fixed : bodyparts (shape, skin, eyes and hair) could not be shared. Keep in mind that bodyparts cannot be removed, only replaced by other bodyparts.
    - fixed : a way to cheat through the "no edit" restriction (thanks Katie Paine !).
    - fixed : "fartouch" was not preventing from clicking on objects that use the touch_end() LSL event (thanks Ibrew Meads !).


1.11e by Henri Beauchamp:
- Inclusion of Boy Lane's fix to a blocked IMs cosmetic bug.


1.11d by Henri Beauchamp:
- Inclusion of Boy Lane's fix to group IMs failing to be blocked.


1.11c by Henri Beauchamp (equivalent to Marine's v1.11.3):
- Inclusion of Marine Kelley's fix to v1.11 crash bugs at login time.


1.11b by Henri Beauchamp:
- Fixed the systematic camera zooming/paning bug when entering build mode.


1.11a by Henri Beauchamp (equivalent to Marine's v1.11.2):
- Inclusion of Marine Kelley's changes (from RestrainedLove v1.11 to 1.11.2):
    - added : a way to "share" inventory items. The user can now be forced to attach/detach objects and clothes by putting them in a folder contained in the root shared folder (for now named "#RLV").
    - added : when wearing a shared object, the name of the attachment point is added to its own for future use. It won't do that on non-shared items nor on no-modify items.
    - added : fake Wear option if the object name contains the name of the target attachment point.
    - added : force attach/wear and force detach/unwear by folder name (in the shared root only).
    - added : list shared inventory with "@getinv=nnnn" command.
    - added : always show HUD objects in snapshots when at least one object is locked.
    - added : restrict show mini map and show world map (thanks Maike Short for the code).
    - added : "@fartouch=n" command  to restrict touch on objects farther than 1.5 meters away.
    - fixed : a couple of loopholes including one discovered by Maike Short.
    - known bugs : "bottom left" and "bottom right" HUD locations are going to "bottom", "top left" and "top right" to "top".


1.10i by Henri Beauchamp (this version has been used after a crash bug was found in v1.11a):
- Fixed the systematic camera zooming/panning bug when entering build mode.


1.10h by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from RestrainedLove v1.10.5.2):
    - changed : emotes crunched down to 20 characters instead of 30.
    - fixed : a loophole discovered by Moss Hastings. Thanks Moss !


1.10g by Henri Beauchamp:
- Inclusion of most (all but the fly interdiction) of Marine Kelley's changes (from RestrainedLove v1.10.3 and v1.10.4):
    - changed : Now when an object forces the user to sit on a furniture, it ignores its own @sittp=n restriction for the time of the command. This means that a Serious Shackles Collar which leash is active will still be able to force sit, regardless of the "@sittp=n" restriction issued by the *Leash plugin. It won't override the arms shackles leash, though (to prevent cheating).
    - changed : Admin Commands is now rendered useless, as it was an easy way to cheat around certain restrictions. Thanks Anaxagoras McMillan for the bug report !
    - changed : Removed a potential cheat around the outfit restrictions.
    - added : Can't delete objects you're sitting on (*).
    - added : Can't edit objects someone is sitting on when sit-tp is prevented.
- Changed (*) so that this restriction only applies whenever you are prevented to unsit.


1.10f by Henri Beauchamp:
- When in RestrainedLove mode, make sure it is impossible to use slurls to change the start location (always log in at the last location).


1.10e by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from RestrainedLove v1.10.2):
    - fixed : a bug with @edit and @viewnote. Thanks to those who pointed them out, they were really not easy to spot.
    - added : @getstatus to let the script know what restrictions the avatar is submitted to.


1.10d by Henri Beauchamp:
- Fixes minor bugs in the client debug menu.


1.10c by Henri Beauchamp:
- Lift all restrictions in debug menu options as long as no item is locked.


1.10b by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from RestrainedLove v1.10.1):
    - fixed : @remoutfit=n didn't prevent from replacing clothes, unless @addoutfit=n was set
    - fixed : could stand up with the pie menu and appearance even when prevented
    - added : eyes, hair and shape for addoutfit and remoutfit
- Make sure the HUD zooming is not restricted as long as no RL object is locked.


1.10a by Henri Beauchamp:
- Inclusion of Marine Kelley's changes (from RestrainedLove v1.10):
    - changed: when the avatar is prevented from sending chat, using gestures no more produce "...".
    - fixed : a couple of bugs about rez and edit that were still possible through some non-obvious options even when prevented.
    - added : force sit on an object in-world, given its UUID. Thank you Shinji Lungu !
    - added : prevent standing up from the object you're sitting on. Also removes the "Stand Up" button. A VERY popular request.
    - added : force unsit. Strangely seems to randomly fail. (*)
    - added : prevent adding/removing clothes (all or selectively).
    - added : force removing clothes (*)
    - added : force detaching items (*)
    - added : prevent reading notecards (doesn't close the already open ones and doesn't prevent from receiving them, for safety reasons).
    - added : prevent opening inventory (closes all the inventory windows when activated).
    - added : check clothes (gives the list of occupied layers, not the names of the clothes for privacy reasons).
    - added : check attachments (gives the list of occupied attachment points, not the names of the items for privacy reasons).
    - added : prevent sending messages on non-public chat channels, with exceptions. Doesn't prevent the "@version=nnnn" automatic reply.
    - added : prevent customizing the TP invites when prevented from sending IMs.
    - added : prevent reading the customized TP invites when prevented from reading IMs.
    - added : ability for the viewer to execute several commands at the same time, separated by commas, only the first one beginning with '@'.
    - added : "garbage collector" : when you unrez an item, all the restrictions attached to it are automatically lifted after a moment.
    - added : commands are delayed until the avatar is fully operational when logging on, to avoid some "race conditions", typically when force-sitting on relog.
    (*) Silently discarded if the user is prevented from doing so by the corresponding restriction. This is on purpose.
        Ex : Force detach won't work if the object is nondetachable. Force undress won't work if the user is prevented from undressing.
- Do not forbid debug features in menus when RestrainedLove is not enabled.


1.05c by Henri Beauchamp:
- When in RestrainedLove mode, make sure it is impossible (including by using the preferences menu or SLURLs), to change the start location (always login in the last location).


1.05b by Henri Beauchamp:
- Integrate the changes from Marine's v1.04:
    - fixed Copy & Wear bug. It was working before, then stopped working. Works again. Thanks LXIX Tomorrow.
    - now @recvim also prevents the user from receiving group chat. A VERY popular request.
- Prevent the use of the debug tool "Dump All Attachments" when the viewer is in RestrainedLove mode.


1.05a by Henri Beauchamp:
- Gag bug fixed (all the text was suppressed in v1.03, preventing the toys to trigger their retorsion measures).
- The start location pull down menu is now suppressed altogether from the login screen when in RestrainedLove mode.


1.04a:
- Fixes the bug in which @clear was clearing all the RestrainedLove settings for all the attachments (instead of just for the calling attachment).


1.03:
- Manual @version checking in IM is now totally silent to the user so they never know when they're checked :)
- Slashed commands like "/ao off", "/hug X" on channel 0 allowed even when prevented from chatting (max 7 characters including '/')
- @edit and @rez viewer commands : to prevent Editing and Rezzing stuff respectively (useful for cages and very hard restraints)
- Can hear owner's attachments messages even when prevented from hearing chat. Cannot hear in-world objects nor other's attachments.
- Crash bug fixed in @clear commands.


1.02 by Henri Beauchamp:
- OOC bug fixed.
- Better algorithm for deciding whether an emote contains "spoken" text or not.
- Gagged text is now emitted as "..." to allow scripted gags to trigger their own retorsion measures. ;-P
- Emotes are no more truncated after the first period when @emote=add is in force.
- Commands are no more echoed to the main chat, unless RestrainedLoveDebug is set to TRUE.
- More bug fix and code cleanup.


1.01:
- Changed the way emotes are handled when prevented from chatting : now (( )) are authorized, signs like ()"*-_^= will discard the message, otherwise emotes are truncated to 30 chars (unless "@emote=add" command is issued, see below), and if a period is present whatever is after it is discarded
- Added "@emote=<rem/add>" to ignore the truncation when sending or hearing emotes in public chat
- Integrated Henri Beauchamp's fixes and additions (such as being able to switch all the features off after relog if needed). Thank you Henri !
- Fixed modification of inventory of locked attachments, thank you for pointing that out Devious Lei !
- Fixed text going through with chat bubbles (although emotes are totally erased, will be fixed later). Thank you for pointing that out Rylla Jewell !


1.01a by Henri Beauchamp:
- Compiled within v1.18.5.1 for Linux.
- Most restrictions lifted when no locked item is worn.
- Made the viewer switchable (after a restart) between a normal viewer and a RestrainedLove viewer.


1.0:
- Compiled under 1.18.4.3 for Windows
- Added No-teleport (Landmark, Location, Friend + Exceptions)
- Added No-Sit-TP over 1.5 meters away
- Added the patch and custom package source code
- Added the viewer API
- Removed ability to log in where you want => forced to My Last Location
- Removed ability to see in Wireframe as it could be used to cheat through blindfolds
- Fixed emotes going through No-receive-chat, now truncated


1.0b:
- Added No-send-chat, No-receive-chat, No-send-IM, No-receive-IM features. Exceptions can be specified to all these behaviours except No-send-chat, for instance to allow IM reception only from your keyholder.
- Added "@version=<channel>" so a script can expect the viewer to say its version on the specified channel. Useful for automated version checking. Thank you Amethyst Rosencrans for suggesting that solution !


1.0a:
- First release.
