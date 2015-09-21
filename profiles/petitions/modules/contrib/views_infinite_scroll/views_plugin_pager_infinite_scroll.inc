<?php

/**
 * The plugin to handle full pager.
 *
 * @ingroup views_pager_plugins
 */
class views_plugin_pager_infinite_scroll extends views_plugin_pager_full {
  function init(&$view, &$display, $options = array()) {
    parent::init($view, $display, $options);
  }

  function summary_title() {
    return "Infinite Scroll";
    if (!empty($this->options['offset'])) {
      return format_plural($this->options['items_per_page'], 'Infinite scroll pager, @count item, skip @skip', 'Infinite scroll pager, @count items, skip @skip', array('@count' => $this->options['items_per_page'], '@skip' => $this->options['offset']));
    }
    return format_plural($this->options['items_per_page'], 'Infinite scroll pager, @count item', 'Infinite scroll pager, @count items', array('@count' => $this->options['items_per_page']));
  }

  function use_count_query() {
 
    return FALSE;
  }

  function query() {
    parent::query();

    $next_page = TRUE;
    if (!empty($this->options['total_pages'])) {
      if (($this->current_page + 1) >= $this->options['total_pages']) {
        $next_page = FALSE;
      }
    }
    if ($next_page) {;
      $limit = $this->options['items_per_page'];
      $this->view->query->set_limit($limit + 1);
    }
  }

  function post_execute(&$result) {
    if (count($result) > $this->options['items_per_page']) {
      array_pop($result);
      $this->next_page = TRUE;
      global $pager_page_array, $pager_total, $pager_total_items;
      $pager_total[$this->options['id']] = $pager_page_array[$this->options['id']] + 2;
    }
  }

  function render($input) {
    global $base_url;
    $content_selector = '';
    $style_options = $this->view->style_options;
    $items_selector = '';
    $img_path = $base_url . '/' . drupal_get_path('module', 'views_infinite_scroll') . '/images/ajax-loader.gif';
    switch ($this->view->plugin_name) {
      case 'default':
        $content_selector = 'div.view-content';
        $items_selector = '.views-row';
        break;
      case 'grid':
        $content_selector = 'div.view-content > table > tbody';
        $items_selector = 'tr';
        break;
      case 'list':
       if (array_key_exists('wrapper_class', $style_options) && !empty($style_options['wrapper_class'])) {
         $wrapper_class = '.' . $style_options['wrapper_class'];
       } else {
         $wrapper_class = '.item-list';
       }
       $content_selector = 'div.view-content>' . $wrapper_class . ' > *';
        $items_selector = '.views-row';
        break;
      case 'table':
        $content_selector = 'div.view-content > table > tbody';
        $items_selector = 'tr';
        break;
    }
    $pager_theme = views_theme_functions('views_infinite_scroll_pager', $this->view, $this->display);
    return theme($pager_theme, array('tags' => $input, 'quantity' => $this->options['items_per_page'], 'view_name' => $this->view->name, 'current_display' => $this->view->current_display, 'content_selector' => $content_selector, 'items_selector' => $items_selector, 'img_path' => $img_path, 'element' => $this->options['id']));
  }
}
