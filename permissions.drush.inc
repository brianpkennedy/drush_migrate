<?php
/**
 * Implementation of hook_drush_command().
 * Define commands for adding, exporting, and draking role permissions
 */
function permissions_drush_command() {
  $items['permissions-dump-drake'] = array(
    'description' => 'Dump permissions to a drake file',
    'arguments' => array(
      'role' => 'The role you want permissions for',
    ),
    'examples' => array(
      'permissions-add access_content 1' => 'Give anonymous users the ability to access content',
    ),
  );
  $items['permissions-dump'] = array(
    'description' => 'Dump permissions',
    'arguments' => array(
      'role' => 'The role you want permissions for',
    ),
    'examples' => array(
      'permissions-dump site_admin' => 'Dump permissions for the site_admin role.',
    ),
  );
  $items['permissions-add'] = array(
    'description' => 'Dump permissions',
    'arguments' => array(
      'permission' => 'A comma delimited string of permissions you want to add',
      'role' => 'The role you want permissions for',
    ),
    'examples' => array(
      'permissions-add access_content 1' => 'Give anonymous users the ability to access content',
    ),
  );
  $items['permissions-set'] = array(
    'description' => 'Set permissions',
    'arguments' => array(
      'permission' => 'A comma delimited string of permissions you want to add',
      'role' => 'The role you want permissions for',
    ),
    'examples' => array(
      'permissions-add access_content 1' => 'Give anonymous users the ability to access content',
    ),
  );

  /*
   * TODO: add role import/export
   */
  return $items;
}

/**
 * Command callback for dumping permissions to the command line
 */
function drush_permissions_dump(){
  $args = func_get_args();
  $role = $args[0];
  $role_permissions = _drush_permissions_fetch_role_permissions($role);

  print var_dump($role_permissions);
}

/**
 * Command callback for dumping permissions to a drake file.
 * Each line will be look something like:
 * permissions-set "access content, foo, bar, baz" 2
 */
function drush_permissions_dump_drake(){
  $args = func_get_args();
  $role = $args[0];
  $role_permissions = _drush_permissions_fetch_role_permissions($role);
  $commands=array();
  foreach($role_permissions as $rid => $perm) {
    $commands[] = sprintf('permissions-set "%s" %s', $perm, $rid);
  }

  $script_to_write = implode("\n", $commands);
  
  $filename = drush_drake_file(false);
  $fh = fopen($filename, "w");
  fwrite($fh, $script_to_write);
  fclose($fh);

  print sprintf("Created permissions script %s\n", $filename);
}

/**
 * Command callback for dumping permissions to add a set of permissions to a role.
 * Expecting a command like:
 * drush permissions-add "access content" 1
 */
function drush_permissions_add() {
  $args = func_get_args();
  $perm = $args[0];
  $role = $args[1];
  if ($role && $perm && is_numeric($role)){
    $role_permissions = _drush_permissions_fetch_role_permissions($role);

    $permissions = $role_permissions[$role];

    $perm_arr  = explode(", ", $perm);

    foreach ($perm_arr as $pos => $perm_token) {
      if (strpos($permissions, $perm_token)){
        unset($perm_arr[$pos]);
      }
    }

    $new_perm_str = implode(", ", $perm_arr);

    if ($new_perm_str){
      $permissions = sprintf("%s %s" ,$permissions ,$new_perm_str);

      db_query('DELETE FROM {permission} WHERE rid = %d', $role);
      db_query("INSERT INTO {permission} (rid, perm) VALUES (%d, '%s')", $role, $permissions);
    }

    print $permissions;
    print "\n";
  }
}

/*
 * TODO: Implement or remove this function
 * drush permissions-remove access_content 1
 */
function drush_permissions_remove() {
  $args = func_get_args();
}

/**
 * Command callback for dumping permissions to add a set of permissions to a role.
 * Expecting a command like:
 * drush permissions-set "access content" 1
 */
function drush_permissions_set() {
  $args = func_get_args();
  print var_dump($args);
  $perm = $args[0];
  $role = $args[1];
  if ($role && $perm && is_numeric($role)){
    db_query('DELETE FROM {permission} WHERE rid = %d', $role);
    db_query("INSERT INTO {permission} (rid, perm) VALUES (%d, '%s')", $role, $permissions);
  }

  print sprintf("Set rid %s permissions to '%s'", $role, $permissions);
}

/*
 * Very very similar to the permissions page submit handler.
 * Fetch a set of permissions from a role id.
 */
function _drush_permissions_fetch_role_permissions($role){
  if ($role){
    if (is_numeric($role)) {
      $result = db_query('SELECT r.rid, p.perm FROM {role} r LEFT JOIN {permission} p ON r.rid = p.rid WHERE r.rid = %d', $role);
    }
  else {
      $result = db_query('SELECT r.rid, p.perm FROM {role} r LEFT JOIN {permission} p ON r.rid = p.rid WHERE r.name= "%s"', $role);
    }
  }
  else {
    $result = db_query('SELECT r.rid, p.perm FROM {role} r LEFT JOIN {permission} p ON r.rid = p.rid order by name');
  }

  // Compile role array:
  // Add a comma at the end so when searching for a permission, we can
  // always search for "$perm," to make sure we do not confuse
  // permissions that are substrings of each other.
  while ($role = db_fetch_object($result)) {
    $role_permissions[$role->rid] = $role->perm .',';
  }
  ksort($role_permissions);
  return $role_permissions;
}