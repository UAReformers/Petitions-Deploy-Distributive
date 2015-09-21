api = 2
core = 7.x

; Drupal
; -------
;
;  Patch to handle menu rebuild error
;    @see http://drupal.org/node/972536
;
; --------------------------------------
projects[drupal][version] = 7.39
projects[drupal][patch][] = http://drupal.org/files/drupal-menu-int-972536-83-D7.patch

; Petitions installation profile
; -------------------------------
projects[petitions][type] = profile
projects[petitions][download][url] = https://github.com/UAReformers/petitions.git
projects[petitions][download][type] = git