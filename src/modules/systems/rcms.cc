/* vim:ts=4
 *
 * Copyleft 2012…2016  Michał Gawron
 * Marduk Unix Labs, http://mulabs.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Visit http://www.gnu.org/licenses/gpl-3.0.html for more information on licensing.
 */

// Standard:
#include <cstddef>

// Qt:
#include <QtXml/QDomElement>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QGridLayout>

// Xefis:
#include <xefis/config/all.h>
#include <xefis/utility/qdom.h>

// Local:
#include "rcms.h"


XEFIS_REGISTER_MODULE_CLASS ("systems/rcms", RemoteControlManagementSystem)


RemoteControlManagementSystem::RemoteControlManagementSystem (xf::ModuleManager* module_manager, QDomElement const& config):
	Module (module_manager, config)
{
	create_configurator_widget();

	parse_properties (config, {
		// Input:
		{ "home.longitude", _home_longitude, true },
		{ "home.latitude", _home_latitude, true },
		{ "home.altitude-amsl", _home_altitude_amsl, true },
		{ "position.longitude", _position_longitude, true },
		{ "position.latitude", _position_latitude, true },
		{ "position.altitude-amsl", _position_altitude_amsl, true },
		// Output:
		{ "home.distance.vlos", _distance_vlos, false },
		{ "home.distance.ground", _distance_ground, false },
		{ "home.distance.vertical", _distance_vertical, false },
		{ "home.true-direction", _true_home_direction, false },
	});

	_distance_computer.set_callback (std::bind (&RemoteControlManagementSystem::compute_distances_to_home, this));
	_distance_computer.observe ({ &_home_longitude, &_home_latitude, &_home_altitude_amsl,
								  &_position_longitude, &_position_latitude, &_position_altitude_amsl });
}


QWidget*
RemoteControlManagementSystem::configurator_widget() const
{
	return _configurator_widget.get();
}


void
RemoteControlManagementSystem::acquire_home()
{
	if (_position_longitude.valid() && _position_latitude.valid() && _position_altitude_amsl.valid() &&
		_home_longitude.configured() && _home_latitude.configured() && _home_altitude_amsl.configured())
	{
		_home_longitude.copy_from (_position_longitude);
		_home_latitude.copy_from (_position_latitude);
		_home_altitude_amsl.copy_from (_position_altitude_amsl);
		_home_acquired = true;
	}
}


void
RemoteControlManagementSystem::create_configurator_widget()
{
	_configurator_widget = std::make_unique<QWidget>();
	_configurator_widget->setSizePolicy (QSizePolicy::Minimum, QSizePolicy::Minimum);

	QPushButton* acquire_home_button = new QPushButton ("Acquire HOME position", _configurator_widget.get());
	QObject::connect (acquire_home_button, SIGNAL (clicked (bool)), this, SLOT (acquire_home()));

	QGridLayout* layout = new QGridLayout (_configurator_widget.get());
	layout->setMargin (WidgetMargin);
	layout->setSpacing (WidgetSpacing);
	layout->addWidget (acquire_home_button, 0, 0);
	layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding), 0, 1);
	layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding), 1, 0);
}


bool
RemoteControlManagementSystem::home_is_valid() const
{
	return _home_longitude.valid() && _home_latitude.valid() && _home_altitude_amsl.valid();
}


bool
RemoteControlManagementSystem::position_is_valid() const
{
	return _position_longitude.valid() && _position_latitude.valid() && _position_altitude_amsl.valid();
}


void
RemoteControlManagementSystem::data_updated()
{
	if (!home_is_valid())
		acquire_home();

	_distance_computer.data_updated (update_time());
}


void
RemoteControlManagementSystem::compute_distances_to_home()
{
	if (home_is_valid() && position_is_valid())
	{
		using std::sqrt;

		LonLat home (*_home_longitude, *_home_latitude);
		LonLat curr (*_position_longitude, *_position_latitude);
		Length ground_dist = curr.haversine_earth (home);
		Length alt_diff = *_position_altitude_amsl - *_home_altitude_amsl;

		_distance_vertical.write (alt_diff);
		_distance_ground.write (ground_dist);
		_distance_vlos.write (sqrt (ground_dist * ground_dist + alt_diff * alt_diff));
		_true_home_direction.write (xf::floored_mod (curr.initial_bearing (home), 360_deg));
	}
	else
	{
		_distance_vlos.set_nil();
		_distance_ground.set_nil();
		_distance_vertical.set_nil();
		_true_home_direction.set_nil();
	}
}

