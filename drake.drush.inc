<?php
// $Id: variable.drush.inc,v 1.19 2010/03/25 02:48:17 weitzman Exp $


/**
 * Implementation of hook_drush_command().
 *
 * In this hook, you specify which commands your
 * drush module makes available, what it does and
 * description.
 *
 * Notice how this structure closely resembles how
 * you define menu hooks.
 *
 * @return
 *   An associative array describing your command(s).
 */
function drake_drush_command() {
  $items['drake'] = array(
    'description' => 'Execute migration scripts',
    'arguments' => array(
      'version' => 'A specific drake file to execute, if one is not given all scripts will be executed.',
    ),
    'examples' => array(
      'drake all' => 'Run all drake scripts that have not been executed',
      'drake 12345678' => 'Run a specific drake script',
    ),
  );
  $items['drake-file'] = array(
    'description' => 'Create a blank migration script.',
    'examples' => array(
      'drush drake-file' => 'Create a blank migration script.',
    )
  );

  return $items;
}

/**
 * Command callback.
 */
function drush_drake() {
  global $conf;

  $args = func_get_args();

  $drake_num = $args[0] ? $args[0] : 'all';

  $filedir = variable_get("drush_drake_scripts_dir", "scripts/drush_drake");

  if ($drake_num == 'all') {
    $files = scandir($filedir);
    foreach ($files as $file) {
      if(!is_dir($file) && $file != "." && $file != "..") {
        $path = sprintf("%s/%s", $filedir, $file);
        _drush_drake_execute_script($path);
      }
    }
  }
  else {
    if (!strpos($drake_num, ".drake")) {
      $drake_num = $drake_num.".drake";
    }
    $filepath = sprintf("%s/%s",$filedir, $drake_num );
    _drush_drake_execute_script($filepath);
  }
}

function drush_drake_file($command_line=true) {
  $timestamp = time();
  $filedir = variable_get("drush_drake_scripts_dir", "scripts/drush_drake");
  $filename = sprintf("%s/%s.drake",$filedir, $timestamp);
  $handler = fopen($filename, "w");
  fclose($handler);

  if ($command_line) {
    print sprintf("Created %s for migrating\n", $filename);
  }
  else {
   return $filename;
  }
}

function _drush_drake_execute_script($path) {
  $script = file($path);
  foreach ($script as $script_line) {
    $script_line = trim($script_line);
    preg_match_all("/\".*?\"|.+?[\s]|.+?$/", $script_line, $args);
    if (count($args)) {
      $args = $args[0];
    }
    print var_dump($args[0]);
    foreach ($args as $pos => $arg){
      $args[$pos]=trim($arg);
    }
    
    print var_dump($args);
    
    // arrays/objects might be serialized in the script, unserialize it for use.
    foreach ($args as $pos => $arg) {
      $args[$pos] = unserialize($arg) ? unserialize($arg) : $arg;
    }



    drush_set_arguments($args);

    $command = drush_parse_command();

    if ($command) {
      drush_dispatch($command);
    }
  }
  print "Completed execution of ".$path . "\n";
}

/**
 * Parse console arguments.
 */
function _drush_drake_parse_args($args) {
  //$args = drush_get_context('argv');

  static $arg_opts = array('c', 'h', 'u', 'r', 'l', 'i');

  $arguments = $options = array();

  for ($i = 1; $i < count($args); $i++) {
    $opt = $args[$i];
    // Is the arg an option (starting with '-')?
    if ($opt{0} == "-" && strlen($opt) != 1) {
      // Do we have multiple options behind one '-'?
      if (strlen($opt) > 2 && $opt{1} != "-") {
        // Each char becomes a key of its own.
        for ($j = 1; $j < strlen($opt); $j++) {
          $options[substr($opt, $j, 1)] = true;
        }
      }
      // Do we have a longopt (starting with '--')?
      elseif ($opt{1} == "-") {
        if ($pos = strpos($opt, '=')) {
          $options[substr($opt, 2, $pos - 2)] = substr($opt, $pos + 1);
        }
        else {
          $options[substr($opt, 2)] = true;
        }
      }
      else {
        $opt = substr($opt, 1);
        // Check if the current opt is in $arg_opts (= has to be followed by an argument).
        if ((in_array($opt, $arg_opts))) {
          if (($args[$i+1] == NULL) || ($args[$i+1] == "") || ($args[$i + 1]{0} == "-")) {
            drush_set_error('DRUSH_INVALID_INPUT', "Invalid input: -$opt needs to be followed by an argument.");
          }
          $options[$opt] = $args[$i + 1];
          $i++;
        }
        else {
          $options[$opt] = true;
        }
      }
    }
    // If it's not an option, it's a command.
    else {
      $arguments[] = $opt;
    }
  }
  return $arguments;
}