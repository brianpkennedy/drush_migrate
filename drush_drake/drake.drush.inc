<?php
// $Id: drake.drush.inc,v 1.1 2010/08/11 22:05:03 bkennedy Exp $


/*
 * Implementation of hook_drush_command().
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
 * Command callback for drush drake.
 * Load up dir of drush scripts and execute them.
 */
function drush_drake() {
  global $conf;

  $args = func_get_args();

  $drake_num = $args[0] ? $args[0] : 'all';

  $filedir = variable_get("drush_drake_scripts_dir", "scripts/drush_drake");

  $executed_scripts = _drush_drake_get_executed_scripts();
  
  if ($drake_num == 'all') {
    $files = scandir($filedir);
    foreach ($files as $file) {
      if(!is_dir($file) && $file != "." && $file != ".."
      && !in_array(_drush_drake_path_to_drake_version($file), $executed_scripts)) {
        $path = sprintf("%s/%s", $filedir, $file);
        _drush_drake_execute_script($path);
      }
    }
  }
  else if (!in_array(drake_num, $executed_scripts)){
    if (!strpos($drake_num, ".drake")) {
      $drake_num = $drake_num.".drake";
    }
    $filepath = sprintf("%s/%s",$filedir, $drake_num );
    _drush_drake_execute_script($filepath);
  }
}

/*
 * Command callback for drake-file.
 * Generate an empty drake file for migration
 */
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

/*
 * Helper function to execute a script.
 * 
 */
function _drush_drake_execute_script($path) {
  $script = file($path);
  foreach ($script as $script_line) {
    $script_line = trim($script_line);
    preg_match_all("/\".*?\"|.+?[\s]|.+?$/", $script_line, $args);
    if (count($args)) {
      $args = $args[0];
    }
    foreach ($args as $pos => $arg){
      $args[$pos]=trim($arg);
    }
        
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
  _drush_drake_mark_script_executed($path);
  print "Completed execution of ".$path . "\n";
}

/*
 * Helper function to put entry into db so script is only executed once.
 */
function _drush_drake_mark_script_executed($path) {
  $drake_version = _drush_drake_path_to_drake_version($path);
  if ($drake_version){
    db_query("INSERT INTO {drush_drake} (drake_id) VALUES (%d)", $drake_version);
  }
}

/*
 * Helper function to parse filename to drake version.
 */
function _drush_drake_path_to_drake_version($filepath) {
  $drake_version = NULL;
  $matches = array();
  preg_match("/\d+\.drake/", $filepath, $matches);
  if (count($matches)){
    $drake_version = str_replace(".drake", "", $matches[0]);
  }
  return $drake_version;
}

/*
 * Lookup executed scripts
 */
function _drush_drake_get_executed_scripts() {
  $executed_scripts = array();
  $results = db_query("SELECT drake_id FROM {drush_drake}");
  
  foreach ($results as $result) {
    $executed_scripts[] = $result->drake_id;
  }
  
  return $executed_scripts;
}