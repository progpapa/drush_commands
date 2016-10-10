<?php

/**
 * @file
 * Replaces the database of the current site with a dump from pantheon.
 *
 * Usage:
 * 0. You should run this command from a Drupal root directory.
 * 1. Define which Pantheon site you want to access:
 *    - best is to create a $conf['pantheon_site_name'] variable in your
 *      local.settings.php file, e.g. $conf['pantheon_site_name'] = 'atl';
 *    - you can use the --site option
 * 2. Define the backup directory for saving the dumps locally:
 *    - default is DRUPAL_ROOT/../backups/pantheon-dumps/$env
 *    - create a $conf['pantheon_backup_dir'] variable in your
 *      local.settings.php file,
 *      e.g. $conf['pantheon_backup_dir'] = '/full/path/to/somewhere';
 *    - use the --dir option
 *
 * Examples:
 * 1. drush psqlsync live
 *    - grabs the live database from pantheon for the site defined in
 *      $conf['pantheon_site_name'] and replaces your current local database
 *      with it
 */


/**
 * Implements hook_drush_command().
 */
function pantheon_sqlsync_drush_command() {
  $items['pantheon-sqlsync'] = array(
    'description' => 'Grabs a database dump from Pantheon and replaces the database of the current local site.',
    'aliases' => array('psqlsync'),
    'examples' => array(
      'drush psqlsync' => 'Get the dev database from Pantheon for the defined site and use it to replace your current local database.',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    'arguments' => array(
      'environment' => 'The name of the pantheon environment to use. Defaults to "dev"',
    ),
    'options' => array(
      'dir' => 'The dump will be saved here. Will be created if it doesn\'t exist. Defaults to the "pantheon_backup_dir" variable.',
      'site' => 'The name of the site. Defaults to the "pantheon_site_name" variable.',
      // @todo: not yet implemented
      'local' => 'Skip getting dump from Pantheon, use the most recent local dump file.',
    ),
  );
  return $items;
}

/**
* Implements drush_hook_COMMAND_validate()
*/
function drush_pantheon_sqlsync_validate($env = 'dev') {
  $alias_record = drush_sitealias_get_record("@self");
  _pantheon_sqlsync_get_site_name();
  _pantheon_sqlsync_set_backup_dir($alias_record, $env);
}

/**
* Callback implementation for pantheon-sqlsync command.
*/
function drush_pantheon_sqlsync($env = 'dev') {
  $alias_record = drush_sitealias_get_record("@self");
  $dir = _pantheon_sqlsync_set_backup_dir($alias_record, $env, TRUE);
  $site = _pantheon_sqlsync_get_site_name(TRUE);
  drush_log(dt('Using environment: @env', array('@env' => $env)), 'success');

  // Get the dump, die on failure.
  $filename = "$site-$env-" . date("YmdHis") . ".sql.gz";
  if (drush_shell_exec("terminus site backups get --element=database --to=$dir/$filename --latest --site=$site --env=$env")) {
    drush_log(dt('Dump downloaded to @backup_dir.',
        array('@backup_dir' => $dir)), 'success');
  }
  else {
    drush_set_error('PANTHEON_GET_DB',
      dt('Couldn\'t download dump from Pantheon.'));
    die;
  }

  // @todo: an option to skip this would be friendly :)
  drush_shell_exec("drush sql-drop --yes");

  if (drush_shell_exec("gunzip < $dir/$filename | drush sql-cli")) {
    drush_log(dt('Dump imported to Drupal'), 'success');
  }
}

/**
 * Gets the Pantheon site name.
 *
 * @param bool $print_message
 *   Determines if a confirmation message should be printed to the user.
 *
 * @return string
 *   The site name used on Pantheon.
 */
function _pantheon_sqlsync_get_site_name($print_message = FALSE) {
  $site = drush_get_option('site', variable_get('pantheon_site_name', ''));
  if (empty($site)) {
    drush_set_error('PANTHEON_GET_DB',
      dt('No site name. Please set $conf["pantheon_site_name"] in local.settings.php.'));
  }
  if ($print_message) {
    drush_log(dt('Using pantheon site: @psite', array('@psite' => $site)), 'success');
  }
  return $site;
}

/**
 * Gets the local backup directory, the dump will be saved here.
 *
 * @param array $alias
 *   The current site alias as returned by drush_sitealias_get_record().
 * @param string $env
 *   The environment name. Usually one of dev, test, live.
 * @param bool $print_message
 *   Determines if a confirmation message should be printed to the user.
 *
 * @return string
 *   The full path of the local backup directory.
 */
function _pantheon_sqlsync_set_backup_dir($alias, $env, $print_message = FALSE) {
  static $backup_dir = NULL;
  if (is_null($backup_dir)) {
    $backup_dir_default = dirname($alias['root']) . '/backups/pantheon-dumps/' . $env . '/';
    $backup_dir = drush_get_option('dir',
        variable_get('pantheon_backup_dir', $backup_dir_default));
    if (!drush_mkdir($backup_dir, TRUE)) {
      die;
    }
  }
  if ($print_message) {
    drush_log(dt('Dump will be saved to: @backup_dir',
        array('@backup_dir' => $backup_dir)), 'success');
  }
  return $backup_dir;
}