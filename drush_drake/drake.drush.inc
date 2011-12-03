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
      'drake all' => 'Run all drake scripts within all modules that have not been executed',
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
function drush_drake($drake_id = null, $module = null) {
  $installed_profile = variable_get('install_profile');

  if (empty($drake_id)) {
    $drake_id = 'all';
  }
  
  if (empty($module)) {
    $module = $drake_id;
    $drake_id = null;
  }
  
  $existing_scripts = _drush_drake_get_existing_scripts($module);
  $count_executed_scripts = 0;
  
  if ($module == 'all') {
    $executed_scripts = _drush_drake_get_executed_scripts();
    
    foreach ($existing_scripts as $module_name => $scripts) {
      $type = ($module_name == $installed_profile) ? 'profile' : 'module';
      $ret = _drush_drake_execute_run_migrations($module_name, $type, $scripts, isset($executed_scripts[$module_name]) ? $executed_scripts[$module_name] : null);
      
      if ($ret === FALSE) {
        return $ret;
      }
      
      $count_executed_scripts += $ret;
    }
  } else {
    $executed_scripts = _drush_drake_get_executed_scripts($module);
    $type = ($module == $installed_profile) ? 'profile' : 'module';
    
    $ret = _drush_drake_execute_run_migrations($module, $type, isset($existing_scripts[$module]) ? $existing_scripts[$module] : null, isset($executed_scripts[$module]) ? $executed_scripts[$module] : null, $drake_id, TRUE);
      
    if ($ret === FALSE) {
      return $ret;
    }

    $count_executed_scripts = $ret;
  }
  
  if ($count_executed_scripts == 0) {
    drush_log('No migrations had to be run, everything up to date.', 'success');
  } else {
    drush_log('A total count of ' . $count_executed_scripts . ' miration(s) were executed.', 'success');
  }
  
  return TRUE;
}

function _drush_drake_execute_run_migrations($module, $system_item_type, $existing_scripts, $executed_scripts, $up_to_version = NULL, $required = FALSE) {
  $latestVersion = 0;
  $count = 0;
  
  if (!empty($executed_scripts)) {
    sort($executed_scripts);
    $latestVersion = end($executed_scripts);
  }
  
  if (($required || $up_to_version !== NULL) && empty($existing_scripts)) {
    drush_log('No scripts found for module/profile ' . $module . '.', 'error');
    return FALSE;
  }
  
  if (!empty($existing_scripts)) {
    foreach ($existing_scripts as $script) {
      if ($script > $latestVersion && ($up_to_version == NULL || $script <= $up_to_version)) {
        if (_drush_drake_execute_script($script, $module, $system_item_type) == FALSE) {
          drush_log('Executing the script ' . $script . ' of module/profile ' . $module . ' failed!', 'error');
          return FALSE;
        }
      
        $count++;
      }
    }
  }
  
  return $count;
}

function _drush_drake_get_dir($system_item_type, $module) {
  if ($system_item_type == 'profile') {
    return 'profiles' . DIRECTORY_SEPARATOR . $module;
  } else {
    return drupal_get_path($system_item_type, $module);
  }
}

/*
 * Helper function to execute a script.
 * 
 */
function _drush_drake_execute_script($version, $module, $system_item_type = 'module') {
  $path = _drush_drake_get_dir($system_item_type, $module);
  $path .= DIRECTORY_SEPARATOR . 'migrations' . DIRECTORY_SEPARATOR . $version . '.drake';
  $script = file($path);
  $line = 1;

  if (!empty($script)) {
    foreach ($script as $script_line) {
      $script_line = trim($script_line);
      preg_match_all("/\".*?\"|.+?[\s]|.+?$/", $script_line, $args);
      
      if (count($args)) {
        $args = $args[0];
      }
      
      foreach ($args as $pos => $arg){
        $args[$pos] = trim($arg);
      }        
      
      drush_set_context('argv', $args);
      drush_parse_args();
      $parsed_args = drush_get_arguments();
      array_unshift($parsed_args, $args[0]);
      drush_set_arguments($parsed_args);
      $command = drush_parse_command();
      
      if ($command) {
        if (drush_dispatch($command) === FALSE) {
          return FALSE;
        }
      } else {
        drush_log('Drush command in line ' . $line . ' was not parsed successfully!', 'error');
        return FALSE;
      }
      
      $line++;
    }
  
    _drush_drake_mark_script_executed($module, $version);
    drush_log("Completed execution of migration " . $version . ' of module/profile ' . $module . "\n", 'success');
  }
  
  return TRUE;
}

/*
 * Helper function to put entry into db so script is only executed once.
 */
function _drush_drake_mark_script_executed($module, $version) {
  db_insert('drush_drake')->fields(array('drake_id' => $version, 'module' => $module))->execute();
}

function _drush_drake_get_existing_scripts($module) {
    $installed_profile = variable_get('install_profile');
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
    } else if ($installed_profile == $module) {
      $existing_scripts[$module] = _drush_drake_get_existing_scripts_for_system_item($module, 'profile');;
    } else if (isset($enabled_modules[$module])) {
      $existing_scripts[$module] = _drush_drake_get_existing_scripts_for_system_item($module);;
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
  $path = _drush_drake_get_dir($system_item_type, $system_item_name);
  $existing_scripts = array();
  $dir = $path . DIRECTORY_SEPARATOR . 'migrations' . DIRECTORY_SEPARATOR;

  if (file_exists($dir) && is_dir($dir)) {
    drush_log('Scan migrations folder within module/profile ' . $system_item_name . '..', 'notice');
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
function _drush_drake_get_executed_scripts($system_item_name = 'all') {
  $query = db_select('drush_drake', 'd')->fields('d');
  
  if ($system_item_name != 'all') {
    $query->condition('module', $system_item_name, '=');
  }

  $executed_scripts = $query->execute()->fetchAll();
  $grouped_results = array();
  
  foreach ($executed_scripts as $script) {
    $grouped_results[$script->module][] = $script->drake_id;
  }
  
  return $grouped_results;
}