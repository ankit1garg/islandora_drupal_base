<?php
/**
 * @file
 * Theme functions for islandora_paged_tei_seadragon.
 */

/**
 * Implements hook_preprocess_theme().
 */
function template_preprocess_islandora_paged_tei_seadragon_viewer(array &$variables) {
  module_load_include('inc', 'islandora_paged_tei_seadragon', 'includes/utilities');
  module_load_include('inc', 'islandora', 'includes/authtokens');
  module_load_include('inc', 'islandora', 'includes/datastream');

  $variables['pages_populated'] = TRUE;
  $variables['left_pane'] = FALSE;
  $variables['page_pid'] = FALSE;
  $viewer_module_path = drupal_get_path('module', 'islandora_paged_tei_seadragon');
  $manuscript = $variables['object'];
  $transform = $variables['transform_object'];
  $tei = FALSE;
  if (isset($_GET['islandora_paged_content_page'])) {
    foreach ($variables['pages'] as $page_info) {
      if ($page_info['page'] == $_GET['islandora_paged_content_page']) {
        $page_pid = $page_info['pid'];
        break;
      }
    }
  }
  if (!isset($page_pid)) {
    $page_entry = reset($variables['pages']);
    $page_pid = $page_entry['pid'];
  }

  drupal_add_css("$viewer_module_path/css/viewer.css");
  // Only want to display TEI in the event that a associated transform is present.
  if ($transform) {
    $generate = (isset($_GET['generate']) ? $_GET['generate'] : NULL);
    $page_num = (isset($_GET['islandora_paged_content_page']) ? $_GET['islandora_paged_content_page'] : 1);

    // This is the TEI that was loaded on page load (independent of JavaScript updates)
    $tei = islandora_paged_tei_seadragon_do_transform($manuscript, $transform, $page_num, $generate);
  }
  if ($page_pid) {
    $token = islandora_get_object_token($page_pid, 'JP2', 2);

    // Not using a render array because it was adding impeding divs.
    $options = array();
    foreach ($variables['pages'] as $pid => $page_info) {
      $options[$pid] = $page_info['page'];
    }
    $pager_variables = array(
      '#options' => $options,
      '#attributes' => array('id' => array('islandora_paged_tei_seadragon_pager')),
      '#value' => $page_pid,
    );
    $variables['pager'] = '<strong>' . t('Page:') . ' </strong>' . t('!page_selector of @total', array(
      '!page_selector' => theme('select', array('element' => $pager_variables)),
      '@total' => count($variables['pages']),
    ));

    $dsids_to_render = array('PDF', 'JP2', 'TIFF');
    $download_prefix = t('<strong>Download: </strong>');
    $page = islandora_object_load($page_pid);
    $datastreams = array();
    foreach ($dsids_to_render as $key => $dsid) {
      if (isset($page[$dsid]) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $page[$dsid])) {
        $size = islandora_datastream_get_human_readable_size($page[$dsid]);
        $link = l(t("@dsid (@size)", array('@dsid' => $dsid, '@size' => $size)), islandora_datastream_get_url($page[$dsid], 'download'));
        $datastreams[$dsid] = array(
          '#markup' => "$download_prefix$link",
          '#prefix' => '<div id="paged-tei-seadragon-viewer-download-datastream-' . $dsid . '">',
          '#suffix' => '</div>',
        );
      }
      else {
        // Prevents some extra calls from JS, this is a heuristic choice.
        unset($dsids_to_render[$key]);
      }
    }
    drupal_add_js(
      array(
        'islandora_paged_tei_seadragon' => array(
          'page_dsids' => $dsids_to_render,
          'download_prefix' => $download_prefix,
        ),
      ),
      'setting'
    );

    if ($tei) {
      $tei_html_name = str_replace(':', '_', $manuscript->id);

      // If we're using TEI HTML fragments, we'll be reading them from the filesystem
      if ($files_dir = file_stream_wrapper_get_instance_by_uri('public://')) {
        $tei_html_dir = '/teihtml/' . $tei_html_name;
        $tei_html_path = '/' . $files_dir->getDirectoryPath() . $tei_html_dir;

        if (variable_get('islandora_saxon_enable_saxon') != TRUE) {
          $tei_html_path = "empty";
        }
      }

      drupal_add_js(
        array(
          'islandora_paged_tei_tei' => array(
            'populated_tei' => $tei,
            'tei_html_name' => $tei_html_name,
            'tei_html_path' => $tei_html_path,
          ),
        ),
        'setting'
      );
    }
    $variables['datastreams'] = drupal_render($datastreams);

    $variables['clipper'] = theme(
      'islandora_openseadragon_clipper',
      array('pid' => $page_pid)
    );

    $variables['seadragon'] = theme(
      'islandora_openseadragon_viewer',
      array(
        'uri' => url(
          "islandora/object/$page_pid/datastream/JP2/view",
          array('absolute' => TRUE, 'query' => array('token' => $token))
        ),
        'fedora_object' => islandora_object_load($page_pid),
      )
    );

    if (isset($transform['CSS'])) {
      drupal_add_css($transform['CSS']->content, array('type' => 'inline'));
    }
    drupal_add_js("$viewer_module_path/js/update_page.js");

    drupal_add_js("$viewer_module_path/js/tei_toggle.js");

    drupal_add_js('jQuery(document).ready(function () {
      // Start with the TEI transcription hidden
      jQuery("#paged-tei-seadragon-viewer-tei").toggle();
      jQuery("#paged-tei-seadragon-viewer-seadragon-pane").width("98%");

      // Turn on the TEI edited-text toggle button
      jQuery("button#toggle").removeClass("tei-hidden");

      // Then we need to be able to toggle it when clicked
      jQuery("button#toggle").click(function() {
        jQuery(".tei-diplomatic").toggleClass("tei-hidden");
        jQuery(".tei-edited").toggleClass("tei-hidden");
        $button = jQuery("button#toggle");

        if ($button.text() == "Show Edited Text") {
          $button.text("Show Unedited Text");
        } else {
          $button.text("Show Edited Text");
        }
      });
    });', 'inline');

    $variables['button_label'] = t('Transcription');
    $variables['page_pid'] = $page_pid;
  }

  // Set if we have populated pages and TEI.
  if (!$tei && !$page_pid) {
    $variables['pages_populated'] = FALSE;
  }
  if ($tei) {
    $variables['left_pane'] = $tei;
  }
  $variables['tei'] = $tei;
  $variables['tei_pane_class'] = $page_pid ? '' : '-full';
}
