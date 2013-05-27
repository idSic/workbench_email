<?php
/**
 * @file
 * Code for the Workbench Email Module.
 */

/**
 * Implements hook_node_form_alter().
 *
 * Determines the current state and next state. Depending on that criteria
 * builds a form element(s) to allow the user to select users to send
 * an email to.
 *
 * @param array $form
 *   The form array
 *
 * @param array $form_state
 *   The form_state array
 */
function workbench_email_form_node_form_alter(&$form, $form_state) {
  if (workbench_moderation_node_type_moderated($form['type']['#value'])) {
    $available = FALSE;
    $wrapper_id = 'revision_information';
    // Workbench Moderation uses "options" fieldset in favor of "revision information"
    // if "administer roles" perm is given to content moderator.
    if (isset($form['revision_information']['workbench_moderation_state_new'])) {
      $wrapper_id = 'revision_information';
      $available = TRUE;
    }
    else if (isset($form['options']['workbench_moderation_state_new'])) {
      $wrapper_id = 'options';
      $available = TRUE;
    }

    if (!$available) {
      return;
    }

    drupal_add_css(drupal_get_path('module', 'workbench_email') . '/css/workbench_email.css');
    $form[$wrapper_id]['workbench_moderation_state_new']['#ajax'] = array(
      'callback' => 'workbench_email_form_node_callback',
      'wrapper' => 'workbench-email-form-node',
      'effect' => 'fade',
      'event' => 'change',
    );

    $form[$wrapper_id]['workbench_email'] = array(
      '#prefix' => '<div id="workbench-email-form-node">',
      '#suffix' => '</div>',
      '#tree' => TRUE,
    );

    // Determine current state.
    if (isset($form['#node']->workbench_moderation['current']->state)) {
      $current_from_state = $form['#node']->workbench_moderation['current']->state;
    }
    else {
      $current_from_state = variable_get('workbench_moderation_default_state_' . $form['type']['#value'], workbench_moderation_state_none());
    }
    if ($current_from_state == workbench_moderation_state_published()) {
      $current_from_state = workbench_moderation_state_none();
    }

    // Determine form next state for the ajax.
    $form_moderation_state = $current_from_state;
    if (!empty($form_state['values']) &&
      isset($form_state['values']['workbench_moderation_state_new'])) {
      $form_moderation_state = $form_state['values']['workbench_moderation_state_new'];
    }
    if (!empty($form_state['values']) &&
      isset($form_state['values'][$wrapper_id]['workbench_moderation_state_new'])) {
      $form_moderation_state = $form_state['values'][$wrapper_id]['workbench_moderation_state_new'];
    }

    $workbench_emails = workbench_email_get();
    foreach ($workbench_emails as $transition => $email_roles) {
      foreach ($email_roles as $rid => $email_transition) {
        if ($email_transition->from_name == $current_from_state
          && $email_transition->to_name == $form_moderation_state) {
          workbench_email_create_form_element($form, $email_transition);
        }
      }
    }

    $form['actions']['submit']['#submit'][] = 'workbench_email_notification_submit';
  }
}

/**
 *  Ajax callback function, targets the workbench_email container.
 *
 * @param array $form
 *   The form array
 *
 * @param array $form_state
 *   The form_state array
 */
function workbench_email_form_node_callback($form, $form_state) {
  if ($form['options']['#access']) {
    return $form['options']['workbench_email'];
  }
  return $form['revision_information']['workbench_email'];
}


/**
 * Submit handler for the workbench_email form element.
 *
 * Sends emails to the specific users selected in the form element.
 *
 * @param array $form
 *   The form array
 *
 * @param array $form_state
 *   The form_state array
 *
 */
function workbench_email_notification_submit($form, &$form_state) {
  if (isset($form_state['values']['workbench_email'])) {
    ($form['options']['#access'] ? $wrapper_id = 'options' : $wrapper_id = 'revision_information');
    foreach ($form_state['values']['workbench_email'] as $rid => $role_emails) {
      foreach ($role_emails as $email) {
        $email_transition = $form[$wrapper_id]['workbench_email'][$rid]['#hidden'];
        workbench_email_mail_send($email, $email_transition, $form_state['node']);
      }
    }
  }
}

/**
 * Create the email selection form element.
 *
 * Creates the appropriate multi select list for a given role and stores
 * some email transition information into the form element.
 *
 * @param array $form
 *   The form array passed by reference
 *
 * @param object $params
 *   A object containing information relating to the email transition
 */
function workbench_email_create_form_element(&$form, $email_transition) {
  $author = FALSE;
  global $user;
  if ($email_transition->rid == 0) {
    $users[] = $user;
    $author = TRUE;
    $description = t('Select the author for notification');
  }
  else{
    $users = workbench_email_get_users($email_transition->rid);
    $description = t('Select one or more users to notify');
  }

  if ($users) {
    $emails = array();
    foreach ($users as $uid => $account) {
      if ($user->mail != $account->mail) {
        $emails[$account->mail] = $account->name;
      }

      if ($author) {
        $emails[$account->mail] = $account->name;
      }
    }

    if ($emails) {
      if ($author) {
        $role = new stdClass();
        $role->name = 'author';
        $role->rid = 0;
      }
      else {
        $role = user_role_load($email_transition->rid);
      }

      $prefix = '';
      $suffix = '';
      $attributes = array('class' => array('chosen-widget'));
      if ($email_transition->automatic) {
        $prefix = '<div class="workbench-email-hide">';
        $suffix = '</div>';
        $attributes = array();
      }

      ($form['options']['#access'] ? $wrapper_id = 'options' : $wrapper_id = 'revision_information');
      $form[$wrapper_id]['workbench_email'][$role->rid] = array(
        '#type' => 'select',
        '#title' => t("@role_name", array('@role_name' => ucwords($role->name))),
        '#options' => $emails,
        '#description' => $description,
        '#multiple' => TRUE,
        '#hidden' => $email_transition,
        '#attributes' => $attributes,
        '#prefix' => $prefix,
        '#suffix' => $suffix,
        '#auto' => $email_transition->automatic
      );
    }
  }
}