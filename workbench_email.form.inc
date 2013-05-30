<?php
/**
 * @file
 * Code for the Workbench Email Module.
 */

/**
 * Implements hook_form_BASE_FORM_ID_alter().
 *
 * Determines the current state and next state. Depending on that criteria
 * builds a form element(s) to allow the user to select users to send
 * an email to.
 *
 * @param array &$form
 *   The form array
 *
 * @param array $form_state
 *   The form_state array
 */
function workbench_email_form_node_form_alter(&$form, $form_state) {
  if (workbench_moderation_node_type_moderated($form['type']['#value'])) {
    $available = FALSE;
    $wrapper_id = 'revision_information';
    // Workbench Moderation uses "options" fieldset in favor
    // of "revision information" if "administer roles" perm is
    // given to content moderator.
    if (isset($form['revision_information']['workbench_moderation_state_new'])) {
      $wrapper_id = 'revision_information';
      $available = TRUE;
    }
    else {
      if (isset($form['options']['workbench_moderation_state_new'])) {
        $wrapper_id = 'options';
        $available = TRUE;
      }
    }

    if (!$available) {
      return;
    }

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

    // Initialize to the current state.
    $form_moderation_state = $current_from_state;
    if (!empty($form_state['values']) &&
      isset($form_state['values']['workbench_moderation_state_new'])
    ) {
      $form_moderation_state = $form_state['values']['workbench_moderation_state_new'];
    }
    if (!empty($form_state['values']) &&
      isset($form_state['values'][$wrapper_id]['workbench_moderation_state_new'])
    ) {
      $form_moderation_state = $form_state['values'][$wrapper_id]['workbench_moderation_state_new'];
    }

    $workbench_access = FALSE;
    if (module_exists('workbench_access')) {
      $workbench_access = TRUE;
    }

    $workbench_emails = workbench_email_get();
    foreach ($workbench_emails as $transition => $email_roles) {
      foreach ($email_roles as $rid => $email_transition) {
        $editors = array();
        $access = TRUE;
        if ($workbench_access) {
          $editors = workbench_email_get_workbench_access_editors($rid, 'form', array('object' => $form));
          $access = FALSE;
        }

        if (!module_exists('workbench_access')) {
          $access = TRUE;
        }
        else {
          if (module_exists('workbench_access') && $editors) {
            $access = TRUE;
          }
          elseif ($email_transition->rid == WORKBENCH_EMAIL_AUTHOR) {
            $access = TRUE;
          }
        }

        if ($email_transition->from_name == $current_from_state
          && $email_transition->to_name == $form_moderation_state
          && !$email_transition->automatic
          && $access) {
          workbench_email_create_form_element($form, $email_transition, $editors);
        }
      }
    }

    $form['actions']['submit']['#submit'][] = 'workbench_email_notification_submit';
  }
}

/**
 * Ajax callback function, targets the workbench_email container.
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
 *   The form array.
 * @param array $form_state
 *   The form state array
 */
function workbench_email_notification_submit($form, &$form_state) {
  if (isset($form_state['values']['workbench_email'])) {
    global $user;
    ($form['options']['#access'] ? $wrapper_id = 'options' : $wrapper_id = 'revision_information');
    foreach ($form_state['values']['workbench_email'] as $rid => $role_emails) {
      foreach ($role_emails as $email) {
        // If !$email means all users selected.
        if (!$email) {
          $accounts = array();
          $editors = array();
          $author = workbench_email_get_all_transition_users($form_state['node'], $rid, $accounts, $editors);
          if (module_exists('workbench_access')) {
            $editors = workbench_email_get_workbench_access_editors($rid, 'node', array('object' => $form_state['node']));
          }

          if ($editors && $accounts) {
            $emails = workbench_email_filter_workbench_access_editors($rid, $accounts, $editors, $user, $author);
            $email_transition = $form[$wrapper_id]['workbench_email'][$rid]['#hidden'];
            foreach ($emails as $email => $name) {
              workbench_email_mail_send($email, $email_transition, $form_state['node']);
            }
          }
        }
        else {
          $email_transition = $form[$wrapper_id]['workbench_email'][$rid]['#hidden'];
          workbench_email_mail_send($email, $email_transition, $form_state['node']);
        }
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
 * @param object $email_transition
 *   The current transition and its data
 * @param array $editors
 *   The workbench access editors
 */
function workbench_email_create_form_element(&$form, $email_transition, $editors = array()) {
  $author = FALSE;
  global $user;
  if ($email_transition->rid == WORKBENCH_EMAIL_AUTHOR) {
    $users[$user->uid] = $user;
    $author = TRUE;
    $description = t('Select the author for notification');
  }
  else {
    $users = workbench_email_get_users($email_transition->rid);
    $description = t('Select one or more users to notify');
  }

  if ($users) {
    $emails = array();
    foreach ($users as $uid => $account) {
      if ($editors) {
        foreach ($editors as $e_uid => $e_account) {
          if ($e_uid == $uid) {
            // If the current user is a member of a separate role they probably
            // wouldn't want to be notified they just made a transition. This
            // could be a possible setting in the future.
            if ($user->mail != $account->mail) {
              $emails[$account->mail] = $account->name;
            }
            // If the current user is the author and the admin has setup the
            // transition to notify them, then we force it.
            if ($author) {
              $emails[$account->mail] = $account->name;
            }
          }
        }
      }
      else {
        // Same logic as above.
        if ($user->mail != $account->mail) {
          $emails[$account->mail] = $account->name;
        }
        // Same logic as above.
        if ($author) {
          $emails[$account->mail] = $account->name;
        }
      }
    }

    if ($emails) {
      if ($author) {
        $role = workbench_email_get_author_role();
      }
      else {
        $role = user_role_load($email_transition->rid);
        array_unshift($emails, t('-- All Users --'));
      }

      ($form['options']['#access'] ? $wrapper_id = 'options' : $wrapper_id = 'revision_information');
      $form[$wrapper_id]['workbench_email'][$role->rid] = array(
        '#type' => 'select',
        '#title' => t("@role_name", array('@role_name' => ucwords($role->name))),
        '#options' => $emails,
        '#description' => $description,
        '#multiple' => TRUE,
        '#hidden' => $email_transition,
        '#attributes' => array('class' => array('chosen-widget')),
      );

      $user_groups = array('editors' => $editors, 'users' => $users);
      drupal_alter('workbench_email_create_form_element', $form, $email_transition, $user_groups);
    }
  }
}
