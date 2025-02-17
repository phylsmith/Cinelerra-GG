#!/usr/bin/perl

# Helper script to compile Cinelerra blend algebra functions
# Calling: BlendAlgebraCompile.pl [options] <function filename>
# The special option "-API" shows the numeric version of the script itself

# Several important definitions

# BlendAlgebraCompile.pl script API version. Must not be changed !
$cin_ba_api = 1;

# C compiler executable and options, can be redefined on user's demand
$cin_compiler = $ENV{'CIN_CC'};
$cin_compiler = $ENV{'CC'} if $cin_compiler eq '';
# a likely default compiler
$cin_compiler = 'gcc' if $cin_compiler eq '';
# another possible compiler
#$cin_compiler = 'clang' if $cin_compiler eq '';
# a fake compiler for debugging the script itself
#$cin_compiler = 'echo';

# Mandatory compiler options to build dynamically loadable object, don't change
$cin_compopts = '-fPIC';
$cin_ldopts = '-shared';
$cin_ldlibs = '-lm';

# Adjustable compiler options for optimization, debugging, warnings
$cin_optopts = '-O2';
$cin_debopts = '-g';
$cin_warnopts = '-Wall -W -Wno-unused-parameter';
# more possible compiler options
#$cin_optopts = '-Ofast -march=native';
#$cin_debopts = '-ggdb';

# Set these to 1 if want to optimize, debug, or verbose output by default
$opt_opt  = 1;
$opt_deb  = 0;
$opt_verb = 0;

# Text editor executable, can be redefined on user's preferences
$cin_editor = $ENV{'CIN_EDITOR'};
# a likely default editor
$cin_editor = 'emacs' if $cin_editor eq '';
# another possible editors
#$cin_editor = 'konsole -e vim' if $cin_editor eq '';
#$cin_editor = 'xterm -e vi' if $cin_editor eq '';
#$cin_editor = 'xedit' if $cin_editor eq '';

# Background execution; set it empty to execute in foreground and block
$cin_bgr = ' &';

# Nothing to change below this line

# Cinelerra installation path
$cin_dat = $ENV{'CIN_DAT'};

# Cinelerra user's config directory
$cin_config = $ENV{'CIN_CONFIG'};
$cin_config = $ENV{'HOME'}.'/.bcast5'
  if $cin_config eq '' && $ENV{'HOME'} ne '';
$cin_config = '.' if $cin_config eq '';
$me_config = "$cin_config/BlendAlgebraCompile.pl";

sub Usage
{
  print "\nCinelerraGG blend algebra compiler driver usage\n\n";
  print "$0 [options] <ba> compile blend algebra function <ba>\n";
  print "$0 -edit <ba>     open blend function <ba> in editor\n";
  print "$0 -API           output own API version\n";
  print "$0 -h, -help, -?  output this help text\n";
  print "\nCinelerraGG additional blend algebra compiler options\n\n";
  print "-compile   don't check modification time, compile unconditionally\n";
  print "-cfile     don't remove intermediate .c file\n";
  print "-opt       add optimizing options to compiler command line\n";
  print "-debug     add debugging options to compiler command line\n";
  print "-warn      add warning options to compiler command line\n";
  print "-edit      open blend function source in text editor\n";
  print "-verbose   verbose execution";
  print " (active by default)" if $opt_verb;
  print "\n";
  print "-noapi     don't copy $0 to $ENV{HOME}/.bcast5\n";
  print "\nCinelerraGG blend algebra compiler default configuration\n\n";
  print "Compiler command line: $cin_compiler $cin_compopts $cin_ldopts -o <ba>.so <ba> $cin_ldlibs\n";
  print "Optimizing options: $cin_optopts";
  print " (active by default)" if $opt_opt;
  print "\n";
  print "Debugging options: $cin_debopts";
  print " (active by default)" if $opt_deb;
  print "\n";
  print "Warning options: $cin_warnopts\n";
  print "Editor command line: $cin_editor <ba>$cin_bgr\n";
  print "\nRelevant environment variables\n\n";
  if ($ENV{'CIN_CC'} ne '')
  {
    print "CIN_CC=$ENV{CIN_CC}\n";
  }
  else
  {
    print "\$CIN_CC: currently not set, fallback: $cin_compiler\n";
  }
  if ($ENV{'CC'} ne '')
  {
    print "CC=$ENV{CC}\n";
  }
  else
  {
    print "\$CC: currently not set, fallback: $cin_compiler\n";
  }
  if ($ENV{'CIN_EDITOR'} ne '')
  {
    print "CIN_EDITOR=$ENV{CIN_EDITOR}\n";
  }
  else
  {
    print "\$CIN_EDITOR: currently not set, fallback: $cin_editor\n";
  }
  if ($ENV{'CIN_DAT'} ne '')
  {
    print "CIN_DAT=$ENV{CIN_DAT}\n";
  }
  else
  {
    print "\$CIN_DAT: currently not set\n";
  }
  if ($ENV{'CIN_CONFIG'} ne '')
  {
    print "CIN_CONFIG=$ENV{CIN_CONFIG}\n";
  }
  else
  {
    print "\$CIN_CONFIG: currently not set, fallback: $ENV{HOME}/.bcast5\n";
  }
  if ($ENV{'CIN_USERLIB'} ne '')
  {
    print "CIN_USERLIB=$ENV{CIN_USERLIB}\n";
  }
  else
  {
    print "\$CIN_USERLIB: currently not set, fallback: $ENV{HOME}/.bcast5lib\n";
  }
  exit 0;
}

$opt_api = $opt_noapi = 0;
$opt_edit = $opt_compile = $opt_cfile = $opt_warn = 0;
$ba_src = '';

# Scan options...
foreach $arg (@ARGV)
{
  unless ($arg =~ /^-/)
  {
    $ba_src = $arg;			# this is the source to compile
    last;
  }
  if ($arg eq '-API')			# print API version
  {
    $opt_api = 1;
  }
  elsif ($arg eq '-noapi')		# don't copy script to ~/.bcast5
  {
    $opt_noapi = 1;
  }
  elsif ($arg eq '-edit')		# open in text editor
  {
    $opt_edit = 1;
    push @opts, $arg;
  }
  elsif ($arg eq '-compile')		# compile unconditionally
  {
    $opt_compile = 1;
    push @opts, $arg;
  }
  elsif ($arg eq '-cfile')		# don't remove .c file
  {
    $opt_cfile = 1;
    push @opts, $arg;
  }
  elsif ($arg eq '-opt')		# optimize
  {
    $opt_opt = 1;
    push @opts, $arg;
  }
  elsif ($arg eq '-debug')		# debug
  {
    $opt_deb = 1;
    push @opts, $arg;
  }
  elsif ($arg eq '-warn')		# warnings
  {
    $opt_warn = 1;
    push @opts, $arg;
  }
  elsif ($arg eq '-verbose')		# print executed commands
  {
    $opt_verb = 1;
    push @opts, $arg;
  }
  else { Usage(); }			# unknown option
}

# A special internal request: output own API version
if ($opt_api)
{
  print "$cin_ba_api\n";
  exit 0;
}

# If a system (not user's) script instance is executed, and the API versions
# of both scripts do not match, then copy the system script to the user's one
# (making a backup copy of the latter). Then execute it with the same argument.
if (! $opt_noapi && $0 ne $me_config)
{
  $me_api = 0;
  $me_api = `\"$me_config\" -API` if -x $me_config;
  #print "BlendAlgebraCompile: user script=$me_config API=$me_api, need $cin_ba_api\n";
  if ($me_api != $cin_ba_api)
  {
    print "BlendAlgebraCompile: copying $0 to $me_config\n";
    unlink "$me_config.bak" if -f "$me_config.bak";
    rename "$me_config", "$me_config.bak" if -f $me_config;
    system "cp \"$0\" \"$me_config\"";
    system "chmod +x \"$me_config\"";
  }
}

# Do nothing if nothing to compile
if ($ba_src eq '') { Usage(); }
#print "BlendAlgebraCompile: source=$ba_src\n";
unless ($opt_edit || -f $ba_src) { Usage(); }

# Redirection to user configurable script copy
$arg = join ' ', @opts;
if (! $opt_noapi && $0 ne $me_config)
{
  exec "\"$me_config\" $arg \"$ba_src\"" if -x $me_config;
}

# If a user's script instance is executed, do everything by myself
print "BlendAlgebraCompile: executing $0 $arg $ba_src\n" if $opt_verb;

# Call text editor
if ($opt_edit)
{
  print "BlendAlgebraCompile: executing $cin_editor $ba_src$cin_bgr\n";
  system "$cin_editor \"$ba_src\"$cin_bgr";
  exit 0;
}

# Check if the function source is newer than the object
if ($cin_dat ne '') { $ba_start = "$cin_dat/dlfcn/BlendAlgebraStart"; }
else                { $ba_start = "BlendAlgebraStart"; }
$mtime_start = -1;
($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime_start,
 $ctime, $blksize, $blocks) = stat ($ba_start);

($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime_src,
 $ctime, $blksize, $blocks) = stat ($ba_src);
$mtime_src = $mtime_start if $mtime_src < $mtime_start;

$mtime_so = -1;
($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime_so,
 $ctime, $blksize, $blocks) = stat ("$ba_src.so") if -f "$ba_src.so";

if (! $opt_compile && $mtime_so > $mtime_src)
{
  print "BlendAlgebraCompile: $ba_src shared object up to date, exiting\n"
    if $opt_verb;
  exit 0;
}

# Call the compiler now
$cin_compopts .= " $cin_optopts"  if $opt_opt;
$cin_compopts .= " $cin_debopts"  if $opt_deb;
$cin_compopts .= " $cin_warnopts" if $opt_warn;
unlink "$ba_src.c" if -f "$ba_src.c";
unlink "$ba_src.so" if -f "$ba_src.so";
print "BlendAlgebraCompile: executing cat $ba_start $ba_src > $ba_src.c\n"
  if $opt_verb;
system "echo '# 1 \"$ba_start\"' > \"$ba_src.c\"";
system "cat \"$ba_start\" >> \"$ba_src.c\"";
system "echo '# 1 \"$ba_src\"' >> \"$ba_src.c\"";
system "cat \"$ba_src\" >> \"$ba_src.c\"";
print "BlendAlgebraCompile: executing $cin_compiler $cin_compopts $cin_ldopts -o $ba_src.so $ba_src.c $cin_ldlibs\n";
system "$cin_compiler $cin_compopts $cin_ldopts -o \"$ba_src.so\" \"$ba_src.c\" $cin_ldlibs";
unlink "$ba_src.c" if ! $opt_cfile && -f "$ba_src.c";

# And finally return to the caller
exit 0;
