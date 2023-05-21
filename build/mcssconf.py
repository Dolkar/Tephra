# doxygen configuration file with additional options for mcss
# see https://mcss.mosra.cz/documentation/doxygen/#configuration

DOXYFILE = 'Doxyfile-mcss'
MAIN_PROJECT_URL = 'https://github.com/Dolkar/Tephra'
LINKS_NAVBAR1 = [
    ("User Guide", 'user-guide', []),
    ("Examples", 'examples', []),
]
LINKS_NAVBAR2 = [
	("Changelog", 'changelog', []),
    ('<a href=\"https://github.com/Dolkar/Tephra/discussions">Discussions</a>', []),
	("API", 'annotated', []),
]

STYLESHEETS = [
    'https://fonts.googleapis.com/css?family=Source+Sans+Pro:400,400i,600,600i%7CSource+Code+Pro:400,400i,600',
    './mcss/css/m-dark-tephra.css',
    './mcss/css/m-documentation.css',
    './mcss/css/m-theme-dark-tephra.css',
    './mcss/css/m-grid.css',
    './mcss/css/m-components.css',
    './mcss/css/m-layout.css',
    './mcss/css/pygments-dark.css',
    './mcss/css/pygments-console.css'
]
THEME_COLOR = '#2c1b15'