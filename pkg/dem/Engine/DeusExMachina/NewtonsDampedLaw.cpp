/*************************************************************************
 Copyright (C) 2008 by Bruno Chareyre		                         *
*  bruno.chareyre@hmg.inpg.fr      					 *
*                                                                        *
*  This program is free software; it is licensed under the terms of the  *
*  GNU General Public License v2 or later. See file LICENSE for details. *
*************************************************************************/

#include"NewtonsDampedLaw.hpp"
#include<yade/core/MetaBody.hpp>
#include<yade/pkg-common/RigidBodyParameters.hpp>
#include<yade/pkg-common/Momentum.hpp>
#include<yade/pkg-common/Force.hpp>
#include<yade/lib-base/yadeWm3Extra.hpp>

YADE_PLUGIN("NewtonsDampedLaw");

void NewtonsDampedLaw::registerAttributes()
{
	DeusExMachina::registerAttributes(); // register (among other) Engine::label
	REGISTER_ATTRIBUTE(damping);
}


NewtonsDampedLaw::NewtonsDampedLaw()
{
	damping = 0.2;
	forceClassIndex = (new Force)->getClassIndex();
	momentumClassIndex = (new Momentum)->getClassIndex();
}

void NewtonsDampedLaw::applyCondition ( MetaBody * ncb )
{
	FOREACH(const shared_ptr<Body>& b, *ncb->bodies){
		if (!b->isDynamic) continue;
		
		RigidBodyParameters* rb = YADE_CAST<RigidBodyParameters*>(b->physicalParameters.get());
		unsigned int id = b->getId();
		Vector3r& m = ( static_cast<Momentum*> ( ncb->physicalActions->find ( id, momentumClassIndex ).get() ) )->momentum;
		Vector3r& f = ( static_cast<Force*> ( ncb->physicalActions->find ( id, forceClassIndex ).get() ) )->force;

		Real dt = Omega::instance().getTimeStep();

		//Newtons mometum law :
		if ( b->isStandalone() ) rb->angularAcceleration=diagDiv ( m,rb->inertia );
		else if ( b->isClump() ) rb->angularAcceleration+=diagDiv ( m,rb->inertia );
		else
		{ // isClumpMember()
			const shared_ptr<Body>& clump ( Body::byId ( b->clumpId ) );
			RigidBodyParameters* clumpRBP=YADE_CAST<RigidBodyParameters*> ( clump->physicalParameters.get() );
			/* angularAcceleration is reset by ClumpMemberMover engine */
			clumpRBP->angularAcceleration+=diagDiv ( m,clumpRBP->inertia );
		}

		// Newtons force law
		if ( b->isStandalone() ) rb->acceleration=f/rb->mass;
		else if ( b->isClump() )
		{
			// accel for clump reset in Clump::moveMembers, called by ClumpMemberMover engine
			rb->acceleration+=f/rb->mass;
		}
		else
		{ // force applied to a clump member is applied to clump itself
			const shared_ptr<Body>& clump ( Body::byId ( b->clumpId ) );
			RigidBodyParameters* clumpRBP=YADE_CAST<RigidBodyParameters*> ( clump->physicalParameters.get() );
			// accels reset by Clump::moveMembers in last iteration
			clumpRBP->acceleration+=f/clumpRBP->mass;
			clumpRBP->angularAcceleration+=diagDiv ( ( rb->se3.position-clumpRBP->se3.position ).Cross ( f ),clumpRBP->inertia ); //acceleration from torque generated by the force WRT particle centroid on the clump centroid
		}


		if (!b->isClump()) {
			//Damping moments
			for ( int i=0; i<3; ++i )
			{
				rb->angularAcceleration[i] *= 1 - damping*Mathr::Sign ( m[i]*(rb->angularVelocity[i] + (Real) 0.5 *dt*rb->angularAcceleration[i]) );
			}
			//Damping force :
			for ( int i=0; i<3; ++i )
			{
				rb->acceleration[i] *= 1 - damping*Mathr::Sign ( f[i]*(rb->velocity[i] + (Real) 0.5 *dt*rb->acceleration[i]) );
			}
		}
		if(rb->blockedDOFs==0){ /* same as: rb->blockedDOFs==PhysicalParameters::DOF_NONE */
			rb->angularVelocity=rb->angularVelocity+dt*rb->angularAcceleration;
			rb->velocity=rb->velocity+dt*rb->acceleration;
		} else {
			if((rb->blockedDOFs & PhysicalParameters::DOF_X)==0) rb->velocity[0]+=dt*rb->acceleration[0];
			if((rb->blockedDOFs & PhysicalParameters::DOF_Y)==0) rb->velocity[1]+=dt*rb->acceleration[1];
			if((rb->blockedDOFs & PhysicalParameters::DOF_Z)==0) rb->velocity[2]+=dt*rb->acceleration[2];
			if((rb->blockedDOFs & PhysicalParameters::DOF_RX)==0) rb->angularVelocity[0]+=dt*rb->angularAcceleration[0];
			if((rb->blockedDOFs & PhysicalParameters::DOF_RY)==0) rb->angularVelocity[1]+=dt*rb->angularAcceleration[1];
			if((rb->blockedDOFs & PhysicalParameters::DOF_RZ)==0) rb->angularVelocity[2]+=dt*rb->angularAcceleration[2];
		}

		Vector3r axis = rb->angularVelocity;
		Real angle = axis.Normalize();
		Quaternionr q;
		q.FromAxisAngle ( axis,angle*dt );
		rb->se3.orientation = q*rb->se3.orientation;
		rb->se3.orientation.Normalize();

		rb->se3.position += rb->velocity*dt;
	}
}

/*
:09:37] eudoxos2: enum {LOOP1,LOOP2,END}
[16:09:37] eudoxos2: for(int state=LOOP1; state!=END; state++){
[16:09:37] eudoxos2: 	FOREACH(const shared_ptr<Body>& b, rootBody->bodies){
[16:09:38] eudoxos2: 		if(b->isClumpMember() && LOOP1){ [[apply that on b->clumpId]]  }
[16:09:38] eudoxos2: 		if((b->isStandalone && LOOP1) || (b->isClump && LOOP2){ [[damping, newton, integrate]] }
[16:09:38] eudoxos2: 		if(b->isClump() && LOOP 2){ b->moveMembers(); }
[16:09:40] eudoxos2: 		}
[16:09:42] eudoxos2: 	}
[16:09:44] eudoxos2: }*/


