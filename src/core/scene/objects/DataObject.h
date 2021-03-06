///////////////////////////////////////////////////////////////////////////////
//
//  Copyright (2013) Alexander Stukowski
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  OVITO is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef __OVITO_DATA_OBJECT_H
#define __OVITO_DATA_OBJECT_H

#include <core/Core.h>
#include <core/reference/RefTarget.h>
#include <core/animation/TimeInterval.h>
#include <core/scene/pipeline/PipelineFlowState.h>
#include <core/scene/objects/DisplayObject.h>

namespace Ovito { OVITO_BEGIN_INLINE_NAMESPACE(ObjectSystem) OVITO_BEGIN_INLINE_NAMESPACE(Scene)

/**
 * \brief Abstract base class for all objects in the scene.

 * A single DataObject can be referenced by multiple ObjectNode instances.
 */
class OVITO_CORE_EXPORT DataObject : public RefTarget
{
protected:

	/// \brief Constructor.
	DataObject(DataSet* dataset);

public:

	/// \brief Asks the object for its validity interval at the given time.
	/// \param time The animation time at which the validity interval should be computed.
	/// \return The maximum time interval that contains \a time and during which the object is valid.
	///
	/// When computing the validity interval of the object, an implementation of this method
	/// should take validity intervals of all sub-objects and sub-controller into account.
	///
	/// The default implementation return TimeInterval::infinite().
	virtual TimeInterval objectValidity(TimePoint time) { return TimeInterval::infinite(); }

	/// \brief This asks the object whether it supports the conversion to another object type.
	/// \param objectClass The destination type. This must be a DataObject derived class.
	/// \return \c true if this object can be converted to the requested type given by \a objectClass or any sub-class thereof.
	///         \c false if the conversion is not possible.
	///
	/// The default implementation returns \c true if the class \a objectClass is the source object type or any derived type.
	/// This is the trivial case: It requires no real conversion at all.
	///
	/// Sub-classes should override this method to allow the conversion to a MeshObject, for example.
	/// When overriding, the base implementation of this method should always be called.
	virtual bool canConvertTo(const OvitoObjectType& objectClass) {
		// Can always convert to itself.
		return this->getOOType().isDerivedFrom(objectClass);
	}

	/// \brief Lets the object convert itself to another object type.
	/// \param objectClass The destination type. This must be a DataObject derived class.
	/// \param time The time at which to convert the object.
	/// \return The newly created object or \c NULL if no conversion is possible.
	///
	/// Whether the object can be converted to the desired destination type can be checked in advance using
	/// the canConvertTo() method.
	///
	/// Sub-classes should override this method to allow the conversion to a MeshObject for example.
	/// When overriding, the base implementation of this method should always be called.
	virtual OORef<DataObject> convertTo(const OvitoObjectType& objectClass, TimePoint time) {
		// Trivial conversion.
		if(this->getOOType().isDerivedFrom(objectClass))
			return this;
		else
			return {};
	}

	/// \brief Lets the object convert itself to another object type.
	/// \param time The time at which to convert the object.
	///
	/// This is a wrapper of the function above using C++ templates.
	/// It just casts the conversion result to the given class.
	template<class T>
	OORef<T> convertTo(TimePoint time) {
		return static_object_cast<T>(convertTo(T::OOType, time));
	}

	/// \brief Asks the object for the result of the geometry pipeline at the given time.
	/// \param time The animation time at which the geometry pipeline is being evaluated.
	/// \return The pipeline flow state generated by this object.
	///
	/// The default implementation just returns the data object itself as the evaluation result.
	virtual PipelineFlowState evaluate(TimePoint time) {
		return PipelineFlowState(this, objectValidity(time));
	}

	/// \brief This function blocks execution until the object is able ready to
	///        provide data via its evaluate() function.
	/// \param time The animation time at which the object should be evaluated.
	/// \param message The text to be shown to the user while waiting.
	/// \param progressDialog An existing progress dialog to use to show the message.
	///                       If NULL, the function will show its own dialog box.
	/// \return true on success; false if the operation has been canceled by the user.
	bool waitUntilReady(TimePoint time, const QString& message, QProgressDialog* progressDialog = nullptr);

	/// \brief Returns a structure that describes the current status of the object.
	///
	/// The default implementation of this method returns an empty status object
	/// that indicates success (PipelineStatus::Success).
	///
	/// An object should generate a ReferenceEvent::ObjectStatusChanged event when its status has changed.
	virtual PipelineStatus status() const { return PipelineStatus(); }

	/// \brief Returns the list of attached display objects that are responsible for rendering this
	///        data object.
	const QVector<DisplayObject*>& displayObjects() const { return _displayObjects; }

	/// \brief Attaches a display object to this scene object that will be responsible for rendering the
	///        data object.
	void addDisplayObject(DisplayObject* displayObj) { _displayObjects.push_back(displayObj); }

	/// \brief Attaches a display object to this scene object that will be responsible for rendering the
	///        data object.
	void setDisplayObject(DisplayObject* displayObj) {
		_displayObjects.clear();
		_displayObjects.push_back(displayObj);
	}

	/// \brief Returns whether the internal data is saved along with the scene.
	/// \return \c true if the data is stored in the scene file; \c false if the data can be restored from an external file or recomputed.
	bool saveWithScene() const { return _saveWithScene; }

	/// \brief Sets whether the per-particle data is saved along with the scene.
	/// \param on \c true if the data should be stored in the scene file; \c false if the per-particle data can be restored from an external file.
	/// \undoable
	virtual void setSaveWithScene(bool on) { _saveWithScene = on; }

	/// \brief Returns a list of object nodes that have this object as a data source.
	QSet<ObjectNode*> dependentNodes() const;

	/// \brief Returns the current value of the revision counter of this scene object.
	/// This counter is increment every time the object changes.
	unsigned int revisionNumber() const { return _revisionNumber; }

	/// \brief Sends an event to all dependents of this RefTarget.
	/// \param event The notification event to be sent to all dependents of this RefTarget.
	virtual void notifyDependents(ReferenceEvent& event) override;

	/// \brief Sends an event to all dependents of this RefTarget.
	/// \param eventType The event type passed to the ReferenceEvent constructor.
	inline void notifyDependents(ReferenceEvent::Type eventType) {
		RefTarget::notifyDependents(eventType);
	}

public:

	Q_PROPERTY(bool saveWithScene READ saveWithScene WRITE setSaveWithScene);

protected:

	/// Handles reference events sent by reference targets of this object.
	virtual bool referenceEvent(RefTarget* source, ReferenceEvent* event) override;

	/// Saves the class' contents to the given stream.
	virtual void saveToStream(ObjectSaveStream& stream) override;

	/// Loads the class' contents from the given stream.
	virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

	/// The revision counter of this object.
	/// The counter is increment every time the object changes.
	unsigned int _revisionNumber;

	/// Controls whether the internal data is saved along with the scene.
	/// If false, only metadata will be saved in a scene file while the contents get restored
	/// from an external data source or get recomputed.
	PropertyField<bool> _saveWithScene;

	/// The attached display objects that are responsible for rendering this object's data.
	VectorReferenceField<DisplayObject> _displayObjects;

	Q_OBJECT
	OVITO_OBJECT

	Q_CLASSINFO("ClassNameAlias", "SceneObject");	// This for backward compatibility with files written by Ovito 2.4 and older.

	DECLARE_PROPERTY_FIELD(_saveWithScene);
	DECLARE_VECTOR_REFERENCE_FIELD(_displayObjects);
};

OVITO_END_INLINE_NAMESPACE
OVITO_END_INLINE_NAMESPACE
}	// End of namespace

#endif // __OVITO_DATA_OBJECT_H
