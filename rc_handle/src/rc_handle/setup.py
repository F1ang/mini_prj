from glob import glob

from setuptools import find_packages, setup


package_name = 'rc_handle'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml', 'LICENSE']),
        ('share/' + package_name + '/launch', glob('launch/*.launch.py')),
        ('share/' + package_name + '/config', glob('config/*.yaml')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='fyp',
    maintainer_email='fyp@example.com',
    description='Publish raw int16/uint8 joystick states and compatible Joy messages.',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'joy_node = rc_handle.joy_node:main',
            'joy_listener = rc_handle.joy_listener:main',
            'joy_raw_listener = rc_handle.joy_listener:raw_main',
        ],
    },
)
