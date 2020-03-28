Translation Strings Policy
===========================

This document provides guidelines for internationalization of the Bitcoin Core software.

How to translate?
------------------

To mark a message as translatable

- In GUI source code (under `src/qt`): use `tr("...")`

- In non-GUI source code (under `src`): use `_("...")`

No internationalization is used for e.g. developer scripts outside `src`.

Strings to be translated
-------------------------

On a high level, these strings are to be translated:

- GUI strings, anything that appears in a dialog or window

- Command-line option documentation

### GUI strings

Anything that appears to the user in the GUI is to be translated. This includes labels, menu items, button texts, tooltips and window titles.
This includes messages passed to the GUI through the U