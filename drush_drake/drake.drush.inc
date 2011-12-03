<?php
// $Id: drake.drush.inc,v 1.1 2010/08/11 22:05:03 bkennedy Exp $


/*
 * Implementation of hook_drush_command().
 */
function drake_drush_command() {
  $items['drake'] = array(
    'description' => 'Execute migration scripts',
    'arguments' => array(
      'version' => 'A specific drake version to execute, if one is not given all scripts will be executed.',
      'module' => 'Module, profile name or "all"',
    ),
    'examples' => array(
      'drake all' => 'Run drake scripts all within all modules that have not been executed',
      'drake 7100 my_module' => 'Run all drake scripts up to version 7100 within module "my_module"',
      'drake views' => 'Run all drake scripts for the module "views"',
    ),
  );

  return $items;
}

/**
 * Command callback for drush drake.
 * Load up dir of drush scripts and execute them.
 */
function drush_drake($drake_id, $module = null) {
  global $installed_profile;
  $executed_scripts = _drush_drake_get_executed_scripts();
  
  if (empty($module)) {
    $module = $drake_id;
    $drake_id = null;
  }
  
  $existing_scripts = _drush_drake_get_existing_scripts($module);
  $count_executed_scripts = 0;
  
  if ($module == 'all') {
    foreach ($existing_scripts as $module_name => $scripts) {
      $type = ($module_name == $installed_profile) ? 'profile' : 'module';
      $ret = _drush_drake_execute_run_migrations($module_name, $type, $scripts, $executed_scripts[$module]);
      
      if ($ret == FALSE) {
        return $ret;
      }
      
      $count_executed_scripts += $ret;
    }
  } else {
    $type = ($module == $installed_profile) ? 'profile' : 'module';
    
    $ret = _drush_drake_execute_run_migrations($module_name, $type, $scripts, $executed_scripts[$module], $drake_id);
      
    if ($ret == FALSE) {
      return $ret;
    }

    $count_executed_scripts = $ret;
  }
  
  if ($count_executed_scripts == 0) {
    drush_log('No migrations had to be run, everything up to date.', 'success');
  } else {
    drush_log('A total of ' . $count_executed_scripts . ' mirations were executed.', 'success');
  }
  
  return TRUE;
}

function _drush_drake_execute_run_migrations($module, $system_item_type, $existing_scripts, $executed_scripts, $up_to_version = NULL) {
  sort($executed_scripts);
  $latestVersion = end($executed_scripts);
  $count = 0;
  
  if ($up_to_version !== NULL && empty($existing_scripts)) {
    drush_log('No scripts found for module/profile ' . $module . ' to run.', 'error');
    return FALSE;
  }
  
  foreach ($existing_scripts as $script) {
    if ($script > $latestVersion && ($up_to_version == NULL || $script <= $up_to_version)) {
      if (_drush_drake_execute_script($script, $module, $system_item_type) == FALSE) {
        drush_log('Executing the script ' . $script . ' of module/profile ' . $module . ' failed!', 'error');
        return FALSE;
      }
      
      $count++;
    }
  }
  
  return $count;
}


/*
 * Helper function to execute a script.
 * 
 */
function _drush_drake_execute_script($version, $module, $system_item_type = 'module') {
  $path = drupal_get_path($system_item_type, $module) . DIRECTORY_SEPARATOR . 'migrations' . DIRECTORY_SEPARATOR . $version . '.drake';
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

    $command = drush_parse_command();

    if ($command) {
      if (drush_dispatch($command, $args) == FALSE) {
        return FALSE;
      }
    }
  }

  drush_drake_mark_script_executed($version, $module);
  drush_log("Completed execution of migration " . $path . "\n", 'success');
  
  return TRUE;
}

/*
 * Helper function to put entry into db so script is only executed once.
 */
function _drush_drake_mark_script_executed($module, $version) {
  db_insert('drush_drake')->fields(array('drake_id' => $version, 'module' => $module))->execute();
}

function _drush_drake_get_existing_scripts($module) {
    global $installed_profile;
    $enabled_modules = module_list();
    $existing_scripts = array();
    
    if ($module == 'all') {
      foreach ($enabled_modules as $module_name) {
        $scripts = _drush_drake_get_existing_scripts_for_system_item($module_name);
        
        if (!empty($scripts)) {
          $existing_scripts[$module_name] = $scripts;
        }
      }
      
      $scripts = _drush_drake_get_existing_scripts_for_system_item($installed_profile, 'profile');
      
      if (!empty($scripts)) {
        $existing_scripts[$installed_profile] = $scripts;
      }
    } else if (isset($enabled_modules[$module])) {
      $existing_scripts[$module] = _drush_drake_get_existing_scripts_for_system_item($module);;
    } else if ($installed_profile == $module) {
      $existing_scripts[$module] = _drush_drake_get_existing_scripts_for_system_item($module, 'profile');;
    }
    
    return $existing_scripts;
}

/**
 * Gets all existing scripts for the specified system item (module, profile, etc).
 * @param string $system_item_name
 * @param string $system_item_type should be "module" or "profile"
 * @return array
 */
function _drush_drake_get_existing_scripts_for_system_item($system_item_name, $system_item_type = 'module') {
  $path = drupal_get_path($system_item_type, $system_item_name);
  $existing_scripts = array();
  $dir = $path . DIRECTORY_SEPARATOR . 'migrations';
  
  if (file_exists($dir) && is_dir($dir)) {
    $files = scandir($dir);
    
    foreach ($files as $file) {
      if (strrpos($file, '.drake') !== FALSE) {
        $existing_scripts[] = (int)substr($file, 0, strlen($file) - strlen('.drake'));
      }
    }
  }
  
  sort($existing_scripts, SORT_NUMERIC);
  
  return $existing_scripts;
}

/*
 * Gets all executed scripts grouped by module/profile name.
 */
function _drush_drake_get_executed_scripts($system_item_name) {
  $query = db_select('drush_drake', 'd')->fields('d');
  
  if ($module != 'all') {
    $query->condition('module', $system_item_name, '=');
  }
  
  $executed_scripts = $query->execute()->fetchAll();
  $grouped_results = array();
  
  foreach ($executed_scripts as $script) {
    $grouped_results[$script->module][] = $script->drake_id;
  }
  
  return $grouped_results;
}