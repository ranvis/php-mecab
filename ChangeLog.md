XXXX-XX-XX: php-mecab 0.6.0+rv
====================

* Add support for PHP 8.
* Multiple userdic support. (unpolished)
  Use comma `,` as a path separtor.
* Improve compatibility on Windows MSVC build.
* Code for PHP 5 is not updated.
* Drop C90 compiler compatibility.

2015-12-05: php-mecab 0.6.0
===========================

* Add support for PHP 7.
* Add `MeCab` namespace classes and constants.
* Non-namespace classes are not supported on PHP 7.
* Non-namespace constants are not supported on PHP 7.
* Procedural APIs (`mecab_*` functions)  are not supported on PHP 7.
* Non-namespace classes are deprecated on PHP 5.
* PHP versions earlier than 5.3 are no longer supported.
* MeCab versions earlier than 0.99 are no longer supported.
* Removed features:
    * `$filter` callback argument of `mecab_split()` function.
    * The persistent resource.
