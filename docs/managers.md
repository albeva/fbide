Managers
========

FBIde internally uses SDK API to organize its classes. Main reason for this
is that this makes it easy to separate functionality into a dynamic library
should need arise, create plugins etc. This also forces untangling the 
responsibilities.

Manger Loading order
--------------------

    1. Manager - the base class loads  first of all
    2. ConfigManager - loads application configuration from config files
    3. UiManager - creates and builds the basic user interface