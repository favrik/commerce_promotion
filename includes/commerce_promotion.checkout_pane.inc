<?php

/*
 * Checkout pane callback: promotion settings
 */
function commerce_promotion_pane_settings_form($checkout_pane) {
  $form = array();
  return $form;
}

/*
 * Checkout pane callback: promotion checkout form
 */
function commerce_promotion_pane_checkout_form($form, &$form_state, $checkout_pane, $order) {
  // Allow to replace pane content with ajax calls.
  $pane_form = array(
    '#prefix' => '<div id="commerce-checkout-coupon-ajax-wrapper">',
    '#suffix' => '</div>',
  );

  if (isset($form_state['triggering_element']) && $form_state['triggering_element']['#name'] == 'coupon_add') {
    $code = $form_state['input']['commerce_promotion']['coupon_code'];
    $result = commerce_promotion_apply_code($code, $order);

    if ($result['success']) {
      // Clear the field value so that the coupon code does not get
      // resubmitted causing an error when user uses main "Continue to next
      // step" submit.
      $pane_form['coupon_code']['#value'] = '';

      // Reload the order so it is not out of date.
      $order = commerce_order_load($order->order_id);

      // Recalculate discounts.
      commerce_cart_order_refresh($order);

      drupal_set_message(t('Coupon code applied.'));
    } else {
      drupal_set_message($result['error'], 'error');
    }
  }

  if (empty($order->field_commerce_promotions)) {
    // Store the payment methods in the form for validation purposes.
    $pane_form['coupon_code'] = array(
      '#type' => 'textfield',
      '#title' => t('Coupon Code'),
      '#description' => t('Enter your coupon code here.'),
    );

    $pane_form['coupon_add'] = array(
      '#type' => 'button',
      '#value' => t('Add coupon'),
      '#name' => 'coupon_add',
      '#limit_validation_errors' => array(),
      // Fix AJAX later.
      '#ajax' => array(
        'callback' => 'commerce_promotion_add_coupon_callback',
        'wrapper' => 'commerce-checkout-coupon-ajax-wrapper',
      ),
    );
  } else {
    // Show added promotion.
    $pane_form['redeemed'] = array(
      '#type' => 'markup',
      '#markup' => commerce_promotion_view_promotion($order),
    );
  }

  // Display any new status messages added by this pane within the pane's area.
  if (drupal_get_messages(NULL, FALSE)) {
    $pane_form['status_messages'] = array(
      '#type' => 'markup',
      '#markup' => theme('status_messages'),
      '#weight' => -1,
    );
  }

  return $pane_form;
}

/*
 * AJAX callback: coupon add button.
 */
function commerce_promotion_add_coupon_callback($form, &$form_state) {
  /*
  ctools_include('ajax');
  ctools_add_js('ajax-responder');
  $path = 'checkout/' . $form_state['order']->order_id . '/review';
  $commands[] = ctools_ajax_command_redirect($path);
  // you can also use ctools_ajax_command_reload() –  xurshid29
  print ajax_render($commands);
  drupal_exit();
   */
  return $form['commerce_promotion'];
}

/*
 * Checkout pane callback: coupon checkout review
 */
function commerce_promotion_pane_review($form, $form_state, $checkout_pane, $order) {
  if (!empty($order->field_commerce_promotions)) {
    return commerce_promotion_view_promotion($order);
  }
}

