shroudBNC setting: "user.lean"
------------------------------

If you're going to use an shroudBNC instance for a lot of users (>500) you might
want to consider using "lean" mode in order to improve performance.

When enabled the parser does not parse/process certain messages which are
received from an IRC server. Several modes are defined:

0 - default mode, every line is processed as usual
1 - chanusers' hosts (i.e. ident@host) are not updated (and thus are also not available to TCL scripts)
2 - sbnc does not keep track of channel users at all (i.e. information which the /names command returns)

Using a TCL script you can set the "lean" mode for new users:

internalbind usrcreate set_lean_mode

proc set_lean_mode {username} {
	setbncuser $username lean 2
}
