import sys
import os

# add project root (one level up from notebooks/) to sys.path
project_root = os.path.abspath(os.path.join(os.getcwd(), os.pardir))
if project_root not in sys.path:
    sys.path.insert(0, project_root)