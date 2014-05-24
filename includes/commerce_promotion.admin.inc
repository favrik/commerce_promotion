<?php

/**
 * Page callback: build a form for promotion.
 */
function commerce_promotion_get_form($promotion_id = NULL) {
  $promotion = $promotion_id ? commerce_promotion_load($promotion_id) : commerce_promotion_defaults();
  $form_state['build_info']['args'] = array($promotion);

  return drupal_build_form('commerce_promotion_form', $form_state);
}

/**
 * Form callback: create or edit a promotion.
 */
function commerce_promotion_form($form, &$form_state, $promotion) {
  $form_state['promotion'] = $promotion;

  // This field is required but let's validate it manually so that
  // AJAX interactions are less restrictive (#limit_validation_errors, etc.).
  $form['#attributes'] = array('class' => array('commerce-promotion-form'));

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Promotion Name'),
    '#description' => t('Shown to customers.'),
    '#default_value' => $promotion['name'],
    '#required' => TRUE,
  );

  $form['status'] = array(
    '#type' => 'radios',
    '#title' => t('Status'),
    '#options' => array(1 => t('Enabled'), 0 => t('Disabled')),
    '#default_value' => $promotion['status'],
  );

  $form['activators'] = array(
    '#type' => 'fieldset',
    '#attributes' => array('id' => 'promotion-activators'),
    '#title' => t('Activate when:'),
  );

  commerce_promotion_set_codes_form($form, $form_state, $promotion);

  commerce_promotion_set_date_range_form($form, $form_state, $promotion);

  commerce_promotion_multiple_items_form($form, $form_state, 'rules',
    array('title' => 'Conditions required to apply it', 'add_button' => 'Add Condition'));

  commerce_promotion_multiple_items_form($form, $form_state, 'actions',
    array('title' => 'Actions to perform', 'add_button' => 'Add Action'));

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => isset($promotion['commerce_promotion_id']) ? t('Save Promotion') : t('Create Promotion'),
  );

  return $form;
}

/**
 * Form builder for AJAX forms that can have many different field groups.
 */
function commerce_promotion_multiple_items_form(&$form, &$form_state, $base_name, $display) {
  // Set a handy reference.
  $promotion = $form_state['promotion'];

  $add_button_name = $base_name . '_add_button';

  $form[$base_name] = array(
    '#type' => 'fieldset',
    '#title' => $display['title'],
    '#attributes' => array('id' => $base_name . '-fs'),
  );

  $builder = commerce_promotion_get_builder();
  $builder_name = 'get' . ucfirst($base_name);
  // $builder->getRules(), ->getActions(), etc.
  $config = $builder->$builder_name();
  $form_state[$base_name . '_config'] = $config;

  $form[$base_name][$base_name . '_options'] = array(
    '#prefix' => '<div class="container-inline">',
    '#type' => 'select',
    '#options' => commerce_promotion_get_component_options($config),
    '#default_option' => '',
  );

  $form[$base_name][$base_name . '_add'] = array(
    '#type' => 'button',
    '#value' => $display['add_button'],
    '#name' => $add_button_name,
    '#ajax' => array(
      'callback' => 'commerce_promotion_ajax_'. $base_name .'_refresh',
      'wrapper' => 'promotion-' . $base_name,
    ),
    '#suffix' => '</div>',
    '#limit_validation_errors' => array(
      array($base_name . '_options')
    ),
  );

  $form[$base_name][$base_name . '_values'] = array(
    '#type' => 'container',
    '#tree' => TRUE,
    '#prefix' => '<div id="promotion-'. $base_name .'">',
    '#suffix' => '</div>',
  );

  // Initialize existing components.
  if (!empty($promotion[$base_name]) && !isset($form_state[$base_name . '_count'])) {
    $components = $promotion[$base_name];
    $form_state[$base_name . '_count'] = 0;
    $counter = &$form_state[$base_name . '_count'];
    foreach ($components as $component_name => $config) {
      $rule_form_container = array(
        '#prefix' => '<div class="container-inline promotion-property">',
        '#suffix' => '</div>',
      );

      $component_form = commerce_promotion_get_component_form($base_name, $component_name, $form_state);
      foreach ($component_form as $name => $field) {
        $rule_form_container[$name] = $field;
      }

      $form_state[$base_name . '_values'][$counter] = $rule_form_container;
      $counter++;
    }
  }

  // Add or remove components via AJAX.
  if (isset($form_state['triggering_element'])) {
    if (strpos($form_state['triggering_element']['#name'], $base_name . '-remove') !== FALSE) {
      $id = str_replace($base_name . '-remove-', '', $form_state['triggering_element']['#name']);
      unset($form_state[$base_name . '_values'][$id]);
    }

    if ($form_state['triggering_element']['#name'] == $add_button_name) {
      if (!isset($form_state[$base_name . '_count'])) {
        $form_state[$base_name . '_count'] = 0;
        $form_state[$base_name . '_values'] = array();
      }

      $rule_form_container = array(
        '#prefix' => '<div class="container-inline promotion-property">',
        '#suffix' => '</div>',
      );

      $rule_form = commerce_promotion_get_component_form($base_name, $form_state['values'][$base_name . '_options'], $form_state);
      foreach ($rule_form as $name => $field) {
        $rule_form_container[$name] = $field;
      }

      $form_state[$base_name . '_values'][$form_state[$base_name . '_count']] = $rule_form_container;
      $form_state[$base_name . '_count']++;
    }
  }

  // If there are component values in the state, render them.
  if (isset($form_state[$base_name . '_count'])) {
    for ($i = 0; $i <= $form_state[$base_name . '_count']; $i++) {
      if (isset($form_state[$base_name . '_values'][$i])) {
        foreach ($form_state[$base_name . '_values'][$i] as $key => $field) {
          $form[$base_name][$base_name . '_values'][$i][$key] = $field;
        }
      }
    }
  }
}

/**
 * Return available rules/actions as options for a select field.
 */
function commerce_promotion_get_component_options($config) {
  $options = array();
  foreach ($config as $key => $config) {
    $options[$key] = $config['name'];
  }

  return $options;
}

/**
 * Component can be rules or actions.
 */
function commerce_promotion_get_component_form($base_name, $component_name, $form_state) {
  $config = $form_state[$base_name . '_config'];
  $component_config = $config[$component_name];
  $component_config['component_name'] = $component_name;

  $form_callback = 'commerce_promotion_' . $base_name . '_item_form';
  $form = $form_callback($component_config, $form_state);

  // Add remove button.
  $count = $form_state[$base_name . '_count'];
  $form[$component_name]['remove'] = array(
    '#type' => 'button',
    '#name' => $base_name . '-remove-' . $count,
    '#value' => t('Remove'),
    '#prefix' => ' &nbsp;&nbsp;&nbsp; ',
    '#ajax' => array(
      'callback' => 'commerce_promotion_ajax_'.$base_name.'_refresh',
      'wrapper' => 'promotion-' . $base_name,
    ),
    '#limit_validation_errors' => array(
      array($base_name . '_options')
    ),
  );

  return $form;
}

/**
 * FIXME refactor item forms into one set of code to be more DRY.
 */
function commerce_promotion_rules_item_form($config, $form_state) {
  $promotion = $form_state['promotion'];

  $defaults = array();
  if (isset($promotion['rules'][$config['component_name']])) {
    $defaults = $promotion['rules'][$config['component_name']];
  }

  $form['operator'] = array(
    '#type' => 'select',
    '#options' => $config['operators'],
    '#field_prefix' => $config['name'],
    '#default_value' => $defaults ? $defaults['operator'] : '',
  );

  if ($config['field']) {
    // Quick and dirty!
    if ($config['component_name'] == 'zipcode_is') {
      $form['field'] = array(
        '#type' => 'textarea',
        '#cols' => 30,
        '#description' => t('Enter zipcodes separated by spaces.'),
      );
    } else if ($config['component_name'] == 'user_is') {
      $form['field'] = array(
        '#type' => 'textfield',
        '#autocomplete_path' => 'admin/commerce/promotions/user-autocomplete',
        '#multiple' => TRUE,
        '#size' => 40,
        '#description' => t('Enter comma separated user names.'),
      );
    } else {
      $form['field'] = array(
        '#type' => 'textfield',
        '#field_suffix' => 'USD',
        '#size' => 5,
      );
    }

    $form['field']['#default_value'] = $defaults ? $defaults['value'] : '';
    if ($config['component_name'] == 'order_total') {
      $form['field']['#default_value'] = intval($form['field']['#default_value']) / 100;
    }
  }

  return array($config['component_name'] => $form);
}

function commerce_promotion_actions_item_form($config, $form_state) {
  $promotion = $form_state['promotion'];

  $default_value = '';
  if (isset($promotion['actions'][$config['component_name']])) {
    $defaults = $promotion['actions'][$config['component_name']];
    $default_value = $config['component_name'] == 'discount_fixed_amount'
                   ? $defaults['argument'] / 100 : $defaults['argument'];
  }

  if ($config['field']) {
    $form['field'] = array(
      '#type' => 'textfield',
      '#field_prefix' => $config['name'],
      '#field_suffix' => $config['component_name'] == 'discount_percentage' ? '%' : 'USD',
      '#size' => 5,
      '#default_value' => $default_value,
    );
  } else {

    $form['dummy'] = array(
      '#markup' => $config['name'],
    );
  }

  return array($config['component_name'] => $form);
}

/**
 * AJAX form callback: return updated rules conditions.
 */
function commerce_promotion_ajax_rules_refresh($form, $form_state) {
  return $form['rules']['rules_values'];
}

/**
 * AJAX form callback: return updated rules conditions.
 */
function commerce_promotion_ajax_actions_refresh($form, $form_state) {
  return $form['actions']['actions_values'];
}

/**
 * Page callback: autocomplete multiple user names.
 */
function commerce_promotion_user_autocomplete($string) {
  $matches = array();

  if ($string) {
    $items = array_map('trim', explode(',', $string));
    $last_item = array_pop($items);
    $prefix = !empty($items) ? implode(', ', $items) . ', ' : '';

    $result = db_select('users')->fields('users', array('name'))->condition('name', '%' . db_like($last_item) . '%', 'LIKE')->range(0, 10)->execute();
    foreach ($result as $user) {
      $matches[$prefix . $user->name] = $user->name;
    }
  }

  drupal_json_output($matches);
}

/**
 * Build the promotion code form.
 */
function commerce_promotion_set_codes_form(&$form, &$form_state, $promotion) {
  $form['activators']['code'] = array(
    '#type' => 'checkbox',
    '#title' => t('Has Promotion Code'),
    '#size' => 20,
    '#default_value' => $promotion['has_code'],
  );

  $form['activators']['code_options'] = array(
    '#type' => 'fieldset',
    '#title' => t('Promotion Code'),
    '#states' => array(
      'visible' => array(
        'input[name="code"]' => array('checked' => TRUE),
      ),
    ),
  );

  $codes = &$form['activators']['code_options'];

  $codes['code_type'] = array(
    '#type' => 'radios',
    '#prefix' => '<div class="container-inline inline-radios">',
    '#suffix' => '</div><br />',
    '#options' => array('simple' => 'Code', 'bulk' => 'Bulk Code'),
    '#default_value' => $promotion['code_type'],
  );

  $codes['simple_code'] = array(
    '#type' => 'textfield',
    '#title' => t('Code'),
    '#size' => 20,
    '#states' => array(
      'visible' => array('input[name="code_type"]' => array('value' => 'simple'))
    ),
    '#default_value' => $promotion['code'],
  );

  $bulk_states = array(
    'visible' => array('input[name="code_type"]' => array('value' => 'bulk'))
  );

  $codes['bulk_number'] = array(
    '#type' => 'textfield',
    '#title' => t('Number of Codes to generate'),
    '#size' => 5,
    '#states' => $bulk_states,
    '#description' => t('Bulk Promotion Codes cannot be edited. They can be regenerated as long as no code has been applied.'),
  );

  $codes['bulk_prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('Code Prefix'),
    '#size' => 10,
    '#states' => $bulk_states,
    '#description' => t('For example: "ABC". Codes generated will follow the format "ABCXXXXX", where X is the auto-generated part of the code.'),
  );

  $lengths = range(8, 15);
  $codes['bulk_length'] = array(
    '#type' => 'select',
    '#title' => t('Code Length'),
    '#options' => array_combine($lengths, $lengths),
    '#default_value' => 8,
    '#description' => t('The number of characters selected here will be appended to the code prefix entered above.'),
    '#states' => $bulk_states,
  );

  $codes['usage_limit_per_code'] = array(
    '#type' => 'textfield',
    '#title' => t('Usage limit per code'),
    '#size' => 5,
    '#description' => t('How many times the code can be applied to an order. 0 for unlimited.'),
    '#default_value' => $promotion['usage_limit_per_code'],
  );

  $codes['usage_limit_per_user'] = array(
    '#type' => 'textfield',
    '#title' => t('Usage limit per user'),
    '#size' => 5,
    '#description' => t('How many times the code can be applied by the Order owner. 0 for unlimited.'),
    '#default_value' => $promotion['usage_limit_per_user'],
  );
}

/**
 * Build the date range form.
 */
function commerce_promotion_set_date_range_form(&$form, &$form_state, $promotion) {
  $form['activators']['date'] = array(
    '#type' => 'checkbox',
    '#title' => t('Is within date range'),
    '#size' => 20,
    '#default_value' => $promotion['date']
  );

  $states = array(
    'visible' => array(
      'input[name="date"]' => array('checked' => TRUE),
    ),
  );

  $form['activators']['date_range'] = array(
    '#type' => 'fieldset',
    '#title' => t('Date Range'),
    '#states' => $states,
  );

  $dates = &$form['activators']['date_range'];

  $dates['start'] = array(
    '#type' => 'date_popup',
    '#title' => t('Start Date'),
    '#states' => $states,
    '#date_year_range' => '-0:+10',
    '#date_format' => 'Y-m-d',
    '#default_value' => $promotion['start'],
  );

  $dates['end'] = array(
    '#type' => 'date_popup',
    '#title' => t('End Date'),
    '#states' => $states,
    '#date_year_range' => '-0:+10',
    '#date_format' => 'Y-m-d',
    '#default_value' => $promotion['end'],
  );
}

/**
 * Form validate callback: validate a promotion data structure.
 */
function commerce_promotion_form_validate($form, &$form_state) {
  $values = $form_state['values'];
}

/**
 * Form submit callback: create or edit a promotion.
 */
function commerce_promotion_form_submit(&$form, &$form_state) {
  $values = $form_state['values'];
  if (isset($form_state['promotion']['commerce_promotion_id'])) {
    $values['commerce_promotion_id'] = $form_state['promotion']['commerce_promotion_id'];
  }

  // FYI: This already saves a single promotion code.
  $promotion_id = commerce_promotion_save($values);

  if (isset($values['code']) && $values['code'] && $values['code_type'] == 'bulk') {
    // Batch create promotion codes.

    // If we are editing a promotion, skip this for now.
    if (isset($values['commerce_promotion_id'])) {
      return;
    }

    $params = array(
      'commerce_promotion_id' => $promotion_id,
      'number' => $values['bulk_number'],
      'prefix' => $values['bulk_prefix'],
      'length' => $values['bulk_length'],
      'usage_limit_per_code' => $values['usage_limit_per_code'],
      'usage_limit_per_user' => $values['usage_limit_per_user'],
      'seed' => md5(uniqid()),
    );

    $batch = array(
      'operations' => array(),
      'finished' => 'commerce_promotion_batch_create_finished_codes',
      'file' => drupal_get_path('module', 'commerce_promotion') . '/includes/commerce_promotion.admin.inc',
      'progress_message' => t('Processing ...')
    );

    for ($i = 0; $i < $params['number']; $i++) {
      $params['id'] = $i;
      $batch['operations'][] = array('commerce_promotion_batch_create_codes', array($params));
    }

    batch_set($batch);
  }
}

function commerce_promotion_batch_create_codes($params, &$context) {
  $context['message'] = t('Creating code @n of @max', array('@n' => $params['id'] + 1, '@max' => $params['number']));

  $code = array(
    'commerce_promotion_id' => $params['commerce_promotion_id'],
    'code' => commerce_promotion_generate_code($params),
    'usage_limit_per_code' => $params['usage_limit_per_code'],
    'usage_limit_per_user' => $params['usage_limit_per_user'],
    'bulk' => 1,
  );

  commerce_promotion_save_code($code);

  $context['results'][] = $code['code'];
}

function commerce_promotion_batch_create_finished_codes($success, $results, $operations) {
  if ($success) {
    // Display success message.
    $message = t('Created @n codes.', array('@n' => count($results)));
    drupal_set_message($message);
  }
  else {
    // Report the last error.
    $error_operation = reset($operations);
    $message = t('An error occurred while processing %error_operation with arguments: @arguments', 
      array('%error_operation' => $error_operation[0], '@arguments' => print_r($error_operation[1], TRUE)));

    drupal_set_message($message, 'error');
  }
}

/**
 * Page callback: list promotions.
 */
function commerce_promotion_list_page($search = '') {
  $header = array(
    array('data' => t('Promotion Id'), 'field' => 'commerce_promotion_id'),
    array('data' => t('Promotion Name'), 'field' => 'name'),
    'Active when',
    array('data' => t('Status'), 'field' => 'status'),
    'Operations',
  );
  $promotions = commerce_promotion_get_all($header, $search);

  $rows = array();
  $promotion_ids = array();
  foreach ($promotions as $promotion) {
    $promotion_ids[] = $promotion->commerce_promotion_id;

    $rows[] = array(
      $promotion->commerce_promotion_id,
      $promotion->name,
      commerce_promotion_active_when_text($promotion),
      $promotion->status ? t('Enabled') : t('Disabled'),
      l('Edit', 'admin/commerce/promotions/edit/' . $promotion->commerce_promotion_id) . ' | '
      . l('Delete', 'admin/commerce/promotions/delete/' . $promotion->commerce_promotion_id)
    );
  }

  $form_state = array();
  return render(drupal_build_form('commerce_promotion_search_form', $form_state))
    . theme('table', array('header' => $header, 'rows' => $rows)) . theme('pager');
}

function commerce_promotion_search_form($form, &$form_state) {
  $form = array();

  $form['search'] = array(
    '#prefix' => '<div class="container-inline">',
    '#title' => '',
    '#size' => 40,
    '#type' => 'textfield',
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Search Promotions'),
    '#suffix' => '</div>',
  );

  return $form;
}

function commerce_promotion_search_form_submit($form, &$form_state) {
  $form_state['redirect'] = 'admin/commerce/promotions/search/' . $form_state['values']['search'];
}

function commerce_promotion_active_when_text($promotion) {
  $items = array();
  $code = commerce_promotion_load_one_code($promotion->commerce_promotion_id);
  if ($code) {
    if ($code['bulk']) {
      $text = 'A Bulk Promotion Code is entered';
    } else {
      $text = 'Promotion Code <strong>' . $code['code'] . '</strong> is entered';
    }
    $items[] = $text;
  }

  $dates = commerce_promotion_date_range($promotion);
  if ($dates) {
    $items[] = $dates;
  }

  return theme('item_list', array('items' => $items));
}

function commerce_promotion_date_range($promotion) {
  if ($promotion->start) {
    $start = format_date(strtotime($promotion->start), 'custom', 'F jS, Y g:ia');
  }

  if ($promotion->end) {
    $end = format_date(strtotime($promotion->end), 'custom', 'F jS, Y g:ia');
  }

  if ($promotion->start && $promotion->end) {
    return 'Date is from ' . $start . ' to ' . $end;
  }

  if ($promotion->start) {
    return 'After ' . $start;
  }

  if ($promotion->end) {
    return 'Before ' . $end;
  }

  return FALSE;
}

/**
 * Form callback: delete a promotion.
 */
function commerce_promotion_delete_form($form, &$form_state, $promotion_id) {
  $promotion = commerce_promotion_load($promotion_id);

  $form_state['commerce_promotion'] = $promotion;

  // Ensure this include file is loaded when the form is rebuilt from the cache.
  $form_state['build_info']['files']['form'] = drupal_get_path('module', 'commerce_promotion') . '/includes/commerce_promotion.admin.inc';
  $form['#submit'][] = 'commerce_promotion_delete_form_submit';
  $form = confirm_form($form, t('Are you sure you want to delete the %name promotion?', array('%name' => $promotion['name'])), 'admin/commerce/promotions', '<p>' . t('Deleting this promotion cannot be undone.') . '</p>', t('Delete'), t('Cancel'), 'confirm');

  // Get the orders referenced by the promotion.
  $query = new EntityFieldQuery();
  $query
    ->entityCondition('entity_type', 'commerce_order')
    ->fieldCondition('field_commerce_promotions', 'target_id', $promotion_id);
  $result = $query->execute();

  if (!empty($result)) {
    $description = t('This promotion is referenced by an order and therefore cannot be deleted. Disable it instead.');
    $form['description']['#markup'] .= '<p>' . $description . '</p>';
    $form['actions']['submit']['#disabled'] = TRUE;
  }

  return $form;
}

function commerce_promotion_delete_form_submit($form, &$form_state) {
  $promotion = $form_state['commerce_promotion'];

  if (commerce_promotion_delete($promotion['commerce_promotion_id'])) {
    drupal_set_message(t('%name promotion has been deleted.', array('%name' => $promotion['name'])));
    watchdog('commerce_promotion', 'Deleted promotion %id %name.', array('%name' => $promotion['name'], '%id' => $promotion['commerce_promotion_id']), WATCHDOG_NOTICE);
  }
  else {
    drupal_set_message(t('%name could not be deleted.', array('%name' => $promotion['name'])), 'error');
  }
  $form_state['redirect'] = 'admin/commerce/promotions';
}
