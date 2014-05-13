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

  // Extract the View and display keys from the cart contents pane setting.
  $pane_view = variable_get('commerce_coupon_checkout_pane_view', 'order_coupon_list|checkout');
  if ($pane_view != 'none') {
    list($view_id, $display_id) = explode('|', $pane_view);
    if (!empty($view_id) && !empty($display_id) && views_get_view($view_id)) {
      $pane_form['redeemed_coupons'] = array(
        '#type' => 'markup',
        '#markup' => commerce_embed_view($view_id, $display_id, array($order->order_id)),
      );
    }
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
  // Re-render the cart summary.
  list($view_id, $display_id) = explode('|', variable_get('commerce_cart_contents_pane_view', 'commerce_cart_summary|default'));
  $cart_summary = commerce_embed_view($view_id, $display_id, array($form_state['order']->order_id));
  $commands[] = ajax_command_replace('.view-commerce-cart-summary', $cart_summary);

  // Re-render coupon pane
  $coupon_pane = $form['commerce_promotion'];
  $commands[] = ajax_command_replace('#commerce-checkout-coupon-ajax-wrapper', drupal_render($coupon_pane));

  return array('#type' => 'ajax', '#commands' => $commands);
}

/*
 * Checkout pane callback: coupon checkout review
 */
function commerce_promotion_pane_review($form, $form_state, $checkout_pane, $order) {
  if (!empty($order->commerce_coupons)) {
    // Extract the View and display keys from the cart contents pane setting.
    list($view_id, $display_id) = explode('|', variable_get('commerce_coupon_review_pane_view', 'order_coupon_list|checkout'));

    return commerce_embed_view($view_id, $display_id, array($order->order_id));
  }
}

