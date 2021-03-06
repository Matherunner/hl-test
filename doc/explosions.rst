Explosions
==========

This page details the physics of explosion and explosive weapons, along with some speedrunning tricks that arise of out these aspects of the game. Familiarity with the health and damage system is assumed.

.. _explosion physics:

General physics
---------------

An explosion is a phenomenon in Half-Life that inflicts damage onto surrounding entities. An explosion needs not be visible, though it is normally accompanied with a fiery visual effect. We may describe an explosion in terms of three fundamental properties. Namely, an explosion has an *origin*, a *source damage*, and a *radius*.

Suppose an explosion occurs. Let :math:`D` be its source damage and :math:`R` its radius. Suppose there is an entity adjacent to the explosion origin. From gaming experience, we know that the further away this entity is from the explosion origin, the lower the damage inflicted on this entity. In fact, the game only looks for entities within a sphere of radius :math:`R` from the explosion origin, ignoring all entities beyond. In the implementation, this is achieved by calling ``UTIL_FindEntityInSphere`` with the radius as one of the parameters.

Assume the entity in question is within :math:`R` units from the explosion origin. First, the game traces a line from the explosion origin to the entity's *body target*. Recall from :ref:`entities` that the body target of an entity is usually, but not always, coincident with the entity's origin. Then, the game computes the distance between this entity's body target and the explosion origin as :math:`\ell`. The damage inflicted onto this entity is thus computed to be

.. math:: D \left( 1 - \frac{\ell}{R} \right) \qquad (0 \le \ell \le R)

Observe that the damage inflicted falls off linearly with distance, and not with the square of distance as is the case in the real world. This process is repeated with other entities found within the sphere.

Interestingly, the computed distance :math:`\ell` may not equal to the actual distance between this entity and explosion origin. In particular, if the line trace is startsolid, then the game computes :math:`\ell = 0`. As a result, the damage inflicted on the entity is exactly the source damage of the explosion. Indeed, all entities within the sphere will receive the same damage.

The case where the line trace is startsolid is seemingly impossible to achieve. Fortunately, this edge case is not hard to exploit in game, the act of which is named *nuking* as will be detailed in :ref:`nuking`. The key to understanding how such exploits might work is to observe that the explosion origin may not coincide with the origin of the entity just before it detonates. The exact way the explosion origin is computed depends on the type of entity generating the explosion.

Explosion origin
----------------

Explosions are always associated with a *source entity*. This entity could be a grenade (of which there are three kinds) or an ``env_explosion``.

Denote :math:`\mathbf{r}` the position of the associated entity. When an explosion occurs, the game will trace a line from :math:`A` to :math:`B`. The exact coordinates of these two points depend on the type of the associated source entity, but they are always, in one way or the other, offset from the source entity's origin. In general, we call :math:`\mathbf{c}` the end position from the line trace. If the trace fraction is not 1, the game will modify the position of the source entity. Otherwise, the position will not be changed.

The new position of the source entity is computed to be

.. math:: \mathbf{r}' := \mathbf{c} + \frac{3}{5} (D - 24) \mathbf{\hat{n}}

All numerical constants are hardcoded. Call the coefficient of :math:`\mathbf{\hat{n}}` the *pull out distance*, as per the comments in the implementation in ``ggrenade.cpp``. This is so named because if the source entity is a grenade, it is typically in contact with some plane or ground when it explodes. By modifying the origin this way, the source entity is being pulled out of the plane by that distance. Remarkably, this distance depends on the source damage of the explosion. For instance, MP5 grenades create explosions with a source damage of :math:`D = 100`, therefore MP5 grenades are pulled out of the plane by 45.6 units at detonation.

Subsequently, the source entity will begin to properly explode. The physics driving the rest of this event has been described in :ref:`explosion physics`. Most importantly, the explosion origin is set to be :math:`\mathbf{r}' + \mathbf{\hat{k}}`. Observe how the :math:`\mathbf{\hat{k}}` is added to the entity's origin, the purpose of which is to pull non-contact grenades out of the ground slightly, as noted in the comments. In the implementation, the addition of this term is done in the function responsible for applying explosive damage, namely ``RadiusDamage``. Since all explosion code invoke this function, this term is always added to the origin for any explosion that happens.

Contact grenades
~~~~~~~~~~~~~~~~

A contact grenade is a type of grenade which detonates upon contact with a solid entity. This includes the MP5 grenades and RPGs.

Let :math:`\mathbf{r}` be the origin of a contact grenade moving in space. Assuming the map is closed, the grenade will eventually hit some entity and then detonate. Denote unit vector :math:`\mathbf{\hat{n}}` the normal to the plane on the entity that got hit. Note that at the instant the grenade collides with the plane, its position will be on the plane. Thus at this instant, let :math:`\mathbf{v}` be the velocity of the grenade.

Then, the start and end points of the line trace are given by

.. math::
	\begin{align*}
	A &:= \mathbf{r} - 32 \mathbf{\hat{v}} \\
	B &:= \mathbf{r} + 32 \mathbf{\hat{v}}
	\end{align*}

Here, :math:`A` is 32 units away from the position of the grenade at collision, in the opposite direction of its velocity. And :math:`B` is 32 units away from that position, but in the direction of the velocity. It is easy to imagine that, more often than not, the end position of the line trace will coincide with the grenade position. This line trace will also rarely be startsolid. This is because the grenade has to pass through open space before hitting the plane, and :math:`A` is approximately one of the grenade's past positions.

Timed grenades
~~~~~~~~~~~~~~

Timed grenades are grenades that detonate after a specific amount of time. This includes hand grenades, which explode three seconds after the pin is pulled.

Denote :math:`\mathbf{r}` the origin of a timed grenade. At detonation, the grenade may or may not be lying on a plane. Since the grenade could well be resting on the ground with zero velocity, it does not make sense to use the velocity in computing the start and end points for the line trace. Instead, Valve decided to use :math:`\mathbf{\hat{k}}` to offset those points from the grenade origin. So, we have

.. math::
	\begin{align*}
	A &:= \mathbf{r} + 8 \mathbf{\hat{k}} \\
	B &:= \mathbf{r} - 32 \mathbf{\hat{k}}
	\end{align*}

Now, :math:`A` is simply 8 units above the grenade and :math:`B` is 32 units below the grenade. This means that there is a greater chance that this line trace is startsolid and also that the trace fraction is 1. The former can occur if there is a solid entity above the grenade, while the latter can occur if the grenade is sufficiently high above the ground.

Explosions by ``env_explosion``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Tripmines
~~~~~~~~~

.. _nuking:

Nuking
------

Nuking refers to the trick of placing explosives in locations confined in a particular way so as to disable damage falloff. The result is that, for all entities found within the sphere of radius :math:`R` from the explosion origin, the damage inflicted will be the maximum damage :math:`D`, effectively with :math:`\ell = 0`. The usefulness of this trick is obvious.

It is important to keep in mind that the explosion radius does not change when nuking. Entities outside the sphere will remain untouched by the explosion.
