from setuptools import find_packages, setup


package_name = 'ui_call_sh'


setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages', [f'resource/{package_name}']),
        (f'share/{package_name}', ['package.xml']),
        (f'share/{package_name}/launch', ['launch/ui_call_sh.launch.py']),
        (f'share/{package_name}/scripts', [
            'scripts/start_pipe_system.sh',
            'scripts/trigger_stop_capture.sh',
            'scripts/triger_stop_capture.sh',
            'scripts/end_pipe_postprocess.sh',
            'scripts/stop_pipe_system.sh',
        ]),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='PipelineRobot',
    maintainer_email='dev@example.com',
    description='ROS2 node that executes local whitelisted shell scripts from UI commands.',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'ui_call_sh_node = ui_call_sh.ui_call_sh_node:main',
        ],
    },
)
