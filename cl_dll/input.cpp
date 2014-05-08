// cl.input.c  -- builds an intended movement command to send to the server

//xxxxxx Move bob and pitch drifting code here and other stuff from view if needed

// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.
#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "parsemsg.h"
extern "C"
{
#include "pm_defs.h"
#include "pm_movevars.h"
#include "kbutton.h"
}
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "view.h"
#include "bench.h"
#include <string.h>
#include <ctype.h>
#include "Exports.h"

#include "vgui_TeamFortressViewport.h"


extern int g_iAlive;

extern int g_weaponselect;
extern cl_enginefunc_t gEngfuncs;

// Defined in pm_math.c
extern "C" float anglemod( float a );
extern "C" void PM_CatagorizePosition();
extern "C" void PM_UnDuck();
extern "C" void PM_Friction();

typedef struct tas_cmd_s
{
	double value;
	bool do_it;
} tas_cmd_t;

static const double M_U = 360.0 / 65536;

StrafeType strafetype = Nostrafe;
static float old_backspeed;
static float old_sidespeed;

static double line_origin[2] = {0, 0};
static double line_dir[2] = {0, 0};

static tas_cmd_t do_setpitch = {0, false};
static tas_cmd_t do_setyaw = {0, false};
static tas_cmd_t do_olsshift = {0, false};
static int tas_cjmp = 0;
static int tas_dtap = 0;
static int tas_dwj = 0;
static int tas_lgagst = 0;
static int tas_jb = 0;
static int tas_db4c = 0;
static int tas_db4l = 0;

extern playermove_t *pmove;

void IN_Init (void);
void IN_Move ( float frametime, usercmd_t *cmd);
void IN_Shutdown( void );
void V_Init( void );
void VectorAngles( const float *forward, float *angles );
int CL_ButtonBits( int );

// xxx need client dll function to get and clear impuse
extern cvar_t *in_joystick;

int	in_impulse	= 0;
int	in_cancel	= 0;
int g_bcap = 1;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*lookstrafe;
cvar_t	*lookspring;
cvar_t	*cl_pitchup;
cvar_t	*cl_pitchdown;
cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_backspeed;
cvar_t	*cl_sidespeed;
cvar_t	*cl_movespeedkey;
cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;
cvar_t	*cl_anglespeedkey;
cvar_t	*cl_vsmoothing;

cvar_t *cl_mtype;
cvar_t *cl_db4c_ceil;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/


kbutton_t	in_mlook;
kbutton_t	in_klook;
kbutton_t	in_jlook;
kbutton_t	in_left;
kbutton_t	in_right;
kbutton_t	in_forward;
kbutton_t	in_back;
kbutton_t	in_lookup;
kbutton_t	in_lookdown;
kbutton_t	in_moveleft;
kbutton_t	in_moveright;
kbutton_t	in_strafe;
kbutton_t	in_speed;
kbutton_t	in_use;
kbutton_t	in_jump;
kbutton_t	in_attack;
kbutton_t	in_attack2;
kbutton_t	in_up;
kbutton_t	in_down;
kbutton_t	in_duck;
kbutton_t	in_reload;
kbutton_t	in_alt1;
kbutton_t	in_score;
kbutton_t	in_break;
kbutton_t	in_graph;  // Display the netgraph

typedef struct kblist_s
{
	struct kblist_s *next;
	kbutton_t *pkey;
	char name[32];
} kblist_t;

kblist_t *g_kbkeys = NULL;

/*
============
KB_ConvertString

Removes references to +use and replaces them with the keyname in the output string.  If
 a binding is unfound, then the original text is retained.
NOTE:  Only works for text with +word in it.
============
*/
int KB_ConvertString( char *in, char **ppout )
{
	char sz[ 4096 ];
	char binding[ 64 ];
	char *p;
	char *pOut;
	char *pEnd;
	const char *pBinding;

	if ( !ppout )
		return 0;

	*ppout = NULL;
	p = in;
	pOut = sz;
	while ( *p )
	{
		if ( *p == '+' )
		{
			pEnd = binding;
			while ( *p && ( isalnum( *p ) || ( pEnd == binding ) ) && ( ( pEnd - binding ) < 63 ) )
			{
				*pEnd++ = *p++;
			}

			*pEnd =  '\0';

			pBinding = NULL;
			if ( strlen( binding + 1 ) > 0 )
			{
				// See if there is a binding for binding?
				pBinding = gEngfuncs.Key_LookupBinding( binding + 1 );
			}

			if ( pBinding )
			{
				*pOut++ = '[';
				pEnd = (char *)pBinding;
			}
			else
			{
				pEnd = binding;
			}

			while ( *pEnd )
			{
				*pOut++ = *pEnd++;
			}

			if ( pBinding )
			{
				*pOut++ = ']';
			}
		}
		else
		{
			*pOut++ = *p++;
		}
	}

	*pOut = '\0';

	pOut = ( char * )malloc( strlen( sz ) + 1 );
	strcpy( pOut, sz );
	*ppout = pOut;

	return 1;
}

/*
============
KB_Find

Allows the engine to get a kbutton_t directly ( so it can check +mlook state, etc ) for saving out to .cfg files
============
*/
struct kbutton_s CL_DLLEXPORT *KB_Find( const char *name )
{
//	RecClFindKey(name);

	kblist_t *p;
	p = g_kbkeys;
	while ( p )
	{
		if ( !stricmp( name, p->name ) )
			return p->pkey;

		p = p->next;
	}
	return NULL;
}

/*
============
KB_Add

Add a kbutton_t * to the list of pointers the engine can retrieve via KB_Find
============
*/
void KB_Add( const char *name, kbutton_t *pkb )
{
	kblist_t *p;	
	kbutton_t *kb;

	kb = KB_Find( name );
	
	if ( kb )
		return;

	p = ( kblist_t * )malloc( sizeof( kblist_t ) );
	memset( p, 0, sizeof( *p ) );

	strcpy( p->name, name );
	p->pkey = pkb;

	p->next = g_kbkeys;
	g_kbkeys = p;
}

/*
============
KB_Init

Add kbutton_t definitions that the engine can query if needed
============
*/
void KB_Init( void )
{
	g_kbkeys = NULL;

	KB_Add( "in_graph", &in_graph );
	KB_Add( "in_mlook", &in_mlook );
	KB_Add( "in_jlook", &in_jlook );
}

/*
============
KB_Shutdown

Clear kblist
============
*/
void KB_Shutdown( void )
{
	kblist_t *p, *n;
	p = g_kbkeys;
	while ( p )
	{
		n = p->next;
		free( p );
		p = n;
	}
	g_kbkeys = NULL;
}

// Simulate key up or down from the console.  If we use KeyDown or KeyUp
// functions instead, sometimes the leftover gEngfuncs.Cmd_Argv(1) from the
// previous command can mess things up.
void TasKeyDown(kbutton_t *b)
{
	if (b->down[0] == -1 || b->down[1] == -1 || b->state & 1)
		return;
	if (!b->down[0])
		b->down[0] = -1;
	else if (!b->down[1])
		b->down[1] = -1;
	else
		return;
	b->state |= 1 + 2;
}

void TasKeyUp(kbutton_t *b)
{
	b->down[0] = b->down[1] = 0;
	b->state = 4;
}

/*
============
KeyDown
============
*/
void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;

	c = gEngfuncs.Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key
	
	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		gEngfuncs.Con_DPrintf ("Three keys down for a button '%c' '%c' '%c'!\n", b->down[0], b->down[1], c);
		return;
	}
	
	if (b->state & 1)
		return;		// still down
	b->state |= 1 + 2;	// down + impulse down
}

/*
============
KeyUp
============
*/
void KeyUp (kbutton_t *b)
{
	int		k;
	char	*c;
	
	c = gEngfuncs.Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
	{
		//Con_Printf ("Keys down for button: '%c' '%c' '%c' (%d,%d,%d)!\n", b->down[0], b->down[1], c, b->down[0], b->down[1], c);
		return;		// some other key is still holding it down
	}

	if (!(b->state & 1))
		return;		// still up (this should not happen)

	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

/*
============
HUD_Key_Event

Return 1 to allow engine to process the key, otherwise, act on it as needed
============
*/
int CL_DLLEXPORT HUD_Key_Event( int down, int keynum, const char *pszCurrentBinding )
{
//	RecClKeyEvent(down, keynum, pszCurrentBinding);

	if (gViewPort)
		return gViewPort->KeyInput(down, keynum, pszCurrentBinding);
	
	return 1;
}

void IN_BreakDown( void ) { KeyDown( &in_break );};
void IN_BreakUp( void ) { KeyUp( &in_break ); };
void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_JLookDown (void) {KeyDown(&in_jlook);}
void IN_JLookUp (void) {KeyUp(&in_jlook);}
void IN_MLookDown (void) {KeyDown(&in_mlook);}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}

void IN_ForwardDown(void)
{
	KeyDown(&in_forward);
	gHUD.m_Spectator.HandleButtonsDown( IN_FORWARD );
}

void IN_ForwardUp(void)
{
	KeyUp(&in_forward);
	gHUD.m_Spectator.HandleButtonsUp( IN_FORWARD );
}

void IN_BackDown(void)
{
	KeyDown(&in_back);
	in_back.state |= 32;
	gHUD.m_Spectator.HandleButtonsDown( IN_BACK );
}

void IN_BackUp(void)
{
	KeyUp(&in_back);
	in_back.state &= ~32;
	gHUD.m_Spectator.HandleButtonsUp( IN_BACK );
}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void)
{
	KeyDown(&in_moveleft);
	in_moveleft.state |= 32;
	gHUD.m_Spectator.HandleButtonsDown( IN_MOVELEFT );
}

void IN_MoveleftUp(void)
{
	KeyUp(&in_moveleft);
	in_moveleft.state &= ~32;
	gHUD.m_Spectator.HandleButtonsUp( IN_MOVELEFT );
}

void IN_MoverightDown(void)
{
	KeyDown(&in_moveright);
	in_moveright.state |= 32;
	gHUD.m_Spectator.HandleButtonsDown( IN_MOVERIGHT );
}

void IN_MoverightUp(void)
{
	KeyUp(&in_moveright);
	in_moveright.state &= ~32;
	gHUD.m_Spectator.HandleButtonsUp( IN_MOVERIGHT );
}
void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

// needs capture by hud/vgui also
extern void __CmdFunc_InputPlayerSpecial(void);

void IN_Attack2Down(void) 
{
	KeyDown(&in_attack2);

#ifdef _TFC
	__CmdFunc_InputPlayerSpecial();
#endif

	gHUD.m_Spectator.HandleButtonsDown( IN_ATTACK2 );
}

void IN_Attack2Up(void) {KeyUp(&in_attack2);}
void IN_UseDown (void)
{
	KeyDown(&in_use);
	gHUD.m_Spectator.HandleButtonsDown( IN_USE );
}
void IN_UseUp (void) {KeyUp(&in_use);}
void IN_JumpDown (void)
{
	KeyDown(&in_jump);
	gHUD.m_Spectator.HandleButtonsDown( IN_JUMP );

}
void IN_JumpUp (void) {KeyUp(&in_jump);}
void IN_DuckDown(void)
{
	KeyDown(&in_duck);
	gHUD.m_Spectator.HandleButtonsDown( IN_DUCK );

}
void IN_DuckUp(void) {KeyUp(&in_duck);}
void IN_ReloadDown(void) {KeyDown(&in_reload);}
void IN_ReloadUp(void) {KeyUp(&in_reload);}
void IN_Alt1Down(void) {KeyDown(&in_alt1);}
void IN_Alt1Up(void) {KeyUp(&in_alt1);}
void IN_GraphDown(void) {KeyDown(&in_graph);}
void IN_GraphUp(void) {KeyUp(&in_graph);}

void IN_AttackDown(void)
{
	KeyDown( &in_attack );
	gHUD.m_Spectator.HandleButtonsDown( IN_ATTACK );
}

void IN_AttackUp(void)
{
	KeyUp( &in_attack );
	in_cancel = 0;
}

// Special handling
void IN_Cancel(void)
{
	in_cancel = 1;
}

void IN_Impulse (void)
{
	in_impulse = atoi( gEngfuncs.Cmd_Argv(1) );
}

void IN_ScoreDown(void)
{
	KeyDown(&in_score);
	if ( gViewPort )
	{
		gViewPort->ShowScoreBoard();
	}
}

void IN_ScoreUp(void)
{
	KeyUp(&in_score);
	if ( gViewPort )
	{
		gViewPort->HideScoreBoard();
	}
}

void IN_MLookUp (void)
{
	KeyUp( &in_mlook );
	if ( !( in_mlook.state & 1 ) && lookspring->value )
	{
		V_StartPitchDrift();
	}
}

void IN_LinestrafeDown()
{
	strafetype = Linestrafe;

	line_origin[0] = pmove->origin[0];
	line_origin[1] = pmove->origin[1];
	double speed = hypot(pmove->velocity[0], pmove->velocity[1]);
	if (speed > 0.1)
	{
		line_dir[0] = pmove->velocity[0] / speed;
		line_dir[1] = pmove->velocity[1] / speed;
	}
	else
	{
		float viewangles[3];
		gEngfuncs.GetViewAngles(viewangles);
		line_dir[0] = cos(viewangles[YAW] * M_PI / 180);
		line_dir[1] = sin(viewangles[YAW] * M_PI / 180);
	}
}
void IN_LinestrafeUp()
{
	strafetype = Nostrafe;
	line_dir[0] = 0;
	line_dir[1] = 0;
}
void IN_BackpedalDown()
{
	strafetype = Backpedal;
}
void IN_BackpedalUp()
{
	strafetype = Nostrafe;
}
void IN_LeftstrafeDown() { strafetype = Leftstrafe; }
void IN_LeftstrafeUp() { strafetype = Nostrafe; }
void IN_RightstrafeDown() { strafetype = Rightstrafe; }
void IN_RightstrafeUp() { strafetype = Nostrafe; }
void IN_SetPitch()
{
	do_setpitch.value = atof(gEngfuncs.Cmd_Argv(1));
	do_setpitch.do_it = true;
}
void IN_SetYaw()
{
	do_setyaw.value = atof(gEngfuncs.Cmd_Argv(1));
	do_setyaw.do_it = true;
}
void IN_OLSShift()
{
	do_olsshift.value = atof(gEngfuncs.Cmd_Argv(1));
	do_olsshift.do_it = true;
}
void IN_ContJump()
{
	tas_cjmp = atoi(gEngfuncs.Cmd_Argv(1));
}
void IN_DuckTap()
{
	tas_dtap = atoi(gEngfuncs.Cmd_Argv(1));
}
void IN_DuckWhenJump()
{
	tas_dwj = atoi(gEngfuncs.Cmd_Argv(1));
}
void IN_LGAGST()
{
	tas_lgagst = atoi(gEngfuncs.Cmd_Argv(1));
}
void IN_JumpBug()
{
	tas_jb = atoi(gEngfuncs.Cmd_Argv(1));
}
void IN_DuckB4Col()
{
	tas_db4c = atoi(gEngfuncs.Cmd_Argv(1));
}
void IN_DuckB4Land()
{
	tas_db4l = atoi(gEngfuncs.Cmd_Argv(1));
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val = 0.0;
	int			impulsedown, impulseup, down;
	
	impulsedown = key->state & 2;
	impulseup	= key->state & 4;
	down		= key->state & 1;
	
	if ( impulsedown && !impulseup )
	{
		// pressed and held this frame?
		val = down ? 0.5 : 0.0;
	}

	if ( impulseup && !impulsedown )
	{
		// released this frame?
		val = down ? 0.0 : 0.0;
	}

	if ( !impulsedown && !impulseup )
	{
		// held the entire frame?
		val = down ? 1.0 : 0.0;
	}

	if ( impulsedown && impulseup )
	{
		if ( down )
		{
			// released and re-pressed this frame
			val = 0.75;	
		}
		else
		{
			// pressed and released this frame
			val = 0.25;	
		}
	}

	// clear impulses
	key->state &= 1;		
	return val;
}

void CL_GetLASpeeds(double &L, double &A, double &prevspeed, double &speed)
{
	prevspeed = hypot(pmove->velocity[0], pmove->velocity[1]);
	if (pmove->onground != -1)
	{
		L = pmove->maxspeed;
		A = pmove->movevars->accelerate;
		pmove->velocity[2] = 0;
		PM_Friction();
		speed = hypot(pmove->velocity[0], pmove->velocity[1]);
	}
	else
	{
		L = 30;
		A = pmove->movevars->airaccelerate;
		speed = prevspeed;
	}
}

double CL_AngleOptimal(double speed, double L, double A)
{
	double tmp = L - pmove->frametime * A * pmove->maxspeed;
	if (tmp <= 0)
		return 90;
	else if (tmp <= speed)
		return acos(tmp / speed) * 180 / M_PI;
	else
		return 0;
}

double CL_AngleConstSpeed(double prevspeed, double speed, double L, double A)
{
	double tauMA = pmove->frametime * pmove->maxspeed * A;
	if (pmove->onground != -1)
	{
		double sqdiff = prevspeed * prevspeed - speed * speed;
		double tmp = sqdiff / tauMA;
		if (tmp + tauMA < L + L && speed + speed >= fabs(tmp - tauMA))
		{
			return acos((tmp - tauMA) / (speed + speed)) * 180 / M_PI;
		}
		else
		{
			tmp = sqrt(L * L - sqdiff);
			if (tauMA - L > tmp && speed >= tmp)
				return acos(-tmp / speed) * 180 / M_PI;
		}
	}
	else
	{
		tauMA *= 0.5;
		if (tauMA <= L && speed >= tauMA)
			return acos(-tauMA / speed) * 180 / M_PI;
		else if (speed >= L)
			return acos(-L / speed) * 180 / M_PI;
	}
	return CL_AngleOptimal(speed, L, A);
}

bool CL_IsAirAccelGreater()
{
	double speed_air = hypot(pmove->velocity[0], pmove->velocity[1]);
	double speed_grnd = speed_air;
	if (speed_air >= 0.1)
		speed_grnd *= fmax(1 - fmax(1, pmove->movevars->stopspeed / speed_air) * pmove->friction * pmove->movevars->friction * pmove->frametime, 0);
	double ctheta_air = cos(CL_AngleOptimal(speed_air, 30, pmove->movevars->airaccelerate) * M_PI / 180);
	double ctheta_grnd = cos(CL_AngleOptimal(speed_grnd, pmove->maxspeed, pmove->movevars->accelerate) * M_PI / 180);
	double mu_air = fmin(pmove->frametime * pmove->maxspeed * pmove->movevars->airaccelerate, 30 - speed_air * ctheta_air);
	double mu_grnd = fmin(pmove->frametime * pmove->maxspeed * pmove->movevars->accelerate, pmove->maxspeed - speed_grnd * ctheta_grnd);
	// Square roots are unnecessary here
	speed_air = speed_air * speed_air + mu_air * mu_air + 2 * speed_air * mu_air * ctheta_air;
	speed_grnd = speed_grnd * speed_grnd + mu_grnd * mu_grnd + 2 * speed_grnd * mu_grnd * ctheta_grnd;
	return speed_air >= speed_grnd;
}

float CL_Sidestrafe(float yaw, double speed, double L, double A, double theta, bool right)
{
	double dir = right ? 1 : -1;
	double phi;
	TasKeyUp(&in_forward);
	if (theta >= 67.5)
	{
		TasKeyDown(right ? &in_moveright : &in_moveleft);
		TasKeyUp(right ? &in_moveleft : &in_moveright);
		TasKeyUp(&in_back);
		phi = 90;
	}
	else if (22.5 < theta && theta < 67.5)
	{
		TasKeyDown(right ? &in_moveright : &in_moveleft);
		TasKeyUp(right ? &in_moveleft : &in_moveright);
		TasKeyDown(&in_back);
		cl_backspeed->value = -cl_backspeed->value;
		int side = (right ? in_moveright : in_moveleft).state & 3;
		int back = in_back.state & 3;
		// Make sure phi is _really_ 45 degrees
		if (side != back)
			(side ? cl_sidespeed : cl_backspeed)->value *= 2;
		phi = 45;
	}
	else
	{
		TasKeyUp(right ? &in_moveright : &in_moveleft);
		TasKeyUp(right ? &in_moveleft : &in_moveright);
		TasKeyDown(&in_back);
		cl_backspeed->value = -cl_backspeed->value;
		phi = 0;
	}
	double beta = speed > 0.1 ? atan2(pmove->velocity[1], pmove->velocity[0]) * 180 / M_PI : yaw;
	beta += dir * (phi - theta);
	if (cl_mtype->value == 2)
		return beta;

	double alpha[2];
	double testspd[2];
	alpha[0] = beta;
	alpha[1] = beta + copysign(M_U, beta);

	for (int i = 0; i < 2; i++)
	{
		double ang = (anglemod(alpha[i]) - phi * dir) * M_PI / 180;
		double avec[2] = {cos(ang), sin(ang)};
		double gamma2 = L - pmove->velocity[0] * avec[0] - pmove->velocity[1] * avec[1];
		if (gamma2 < 0)
		{
			testspd[i] = speed;
			continue;
		}
		double mu = fmin(pmove->frametime * pmove->maxspeed * A, gamma2);
		testspd[i] = hypot(pmove->velocity[0] + mu * avec[0], pmove->velocity[1] + mu * avec[1]);
	}

	if (testspd[0] > testspd[1])
		return alpha[0];
	else
		return alpha[1];
}

float CL_Sidestrafe(float yaw, bool right)
{
	double L, A, prevspeed, speed;
	CL_GetLASpeeds(L, A, prevspeed, speed);

	double theta;
	if (speed < 0.1)
		theta = 0;
	else if (cl_mtype->value == 2)
		theta = CL_AngleConstSpeed(prevspeed, speed, L, A);
	else
		theta = CL_AngleOptimal(speed, L, A);

	return CL_Sidestrafe(yaw, speed, L, A, theta, right);
}

double CL_PointToLineDist(const double p[2])
{
	double tmp[2] = {line_origin[0] - p[0], line_origin[1] - p[1]};
	double dotprod = line_dir[0] * tmp[0] + line_dir[1] * tmp[1];
	return hypot(tmp[0] - line_dir[0] * dotprod, tmp[1] - line_dir[1] * dotprod);
}

float CL_Linestrafe(float yaw)
{
	double avec[2], ct, st, gamma2, mu, theta = 0;
	double newpos_sright[2], newpos_sleft[2];
	double L, A, prevspeed, speed;
	CL_GetLASpeeds(L, A, prevspeed, speed);

	if (speed < 0.1)
		return CL_Sidestrafe(yaw, speed, L, A, theta, true);

	if (cl_mtype->value == 2)
		theta = CL_AngleConstSpeed(prevspeed, speed, L, A);
	else
		theta = CL_AngleOptimal(speed, L, A);

	if (do_olsshift.do_it)
	{
		line_origin[0] += do_olsshift.value * line_dir[1];
		line_origin[1] -= do_olsshift.value * line_dir[0];
		do_olsshift.do_it = false;
	}

	ct = cos(theta * M_PI / 180);
	st = sin(theta * M_PI / 180);
	gamma2 = L - speed * ct;
	if (gamma2 < 0)
		mu = 0;
	else
		mu = fmin(pmove->frametime * pmove->maxspeed * A, gamma2) / speed;

	avec[0] = (pmove->velocity[0] * ct + pmove->velocity[1] * st) * mu;
	avec[1] = (-pmove->velocity[0] * st + pmove->velocity[1] * ct) * mu;
	newpos_sright[0] = pmove->origin[0] + pmove->frametime * (pmove->velocity[0] + avec[0]);
	newpos_sright[1] = pmove->origin[1] + pmove->frametime * (pmove->velocity[1] + avec[1]);

	avec[0] = (pmove->velocity[0] * ct - pmove->velocity[1] * st) * mu;
	avec[1] = (pmove->velocity[0] * st + pmove->velocity[1] * ct) * mu;
	newpos_sleft[0] = pmove->origin[0] + pmove->frametime * (pmove->velocity[0] + avec[0]);
	newpos_sleft[1] = pmove->origin[1] + pmove->frametime * (pmove->velocity[1] + avec[1]);

	return CL_Sidestrafe(yaw, speed, L, A, theta, CL_PointToLineDist(newpos_sright) <= CL_PointToLineDist(newpos_sleft));
}

void CL_AnglesAndMoves ( float frametime, float *viewangles, struct usercmd_s *cmd )
{
	float	speed;
	static StrafeType old_strafetype = Nostrafe;
	
	gEngfuncs.GetViewAngles( (float *)viewangles );
	speed = frametime;

	if (do_setyaw.do_it)
		viewangles[YAW] = do_setyaw.value;

	if (strafetype == Nostrafe && !do_setyaw.do_it)
	{
		viewangles[YAW] -= speed*cl_yawspeed->value*CL_KeyState (&in_right);
		viewangles[YAW] += speed*cl_yawspeed->value*CL_KeyState (&in_left);
		float	up, down;
		up = CL_KeyState (&in_lookup);
		down = CL_KeyState(&in_lookdown);
		viewangles[PITCH] -= speed*cl_pitchspeed->value * up;
		viewangles[PITCH] += speed*cl_pitchspeed->value * down;
		if (up || down)
			V_StopPitchDrift ();
	}
	else if (strafetype == Leftstrafe || strafetype == Rightstrafe)
	{
		viewangles[YAW] = CL_Sidestrafe(viewangles[YAW], strafetype == Rightstrafe);
	}
	else if (strafetype == Linestrafe)
	{
		if (do_setyaw.do_it)
		{
			line_dir[0] = cos(do_setyaw.value * M_PI / 180);
			line_dir[1] = sin(do_setyaw.value * M_PI / 180);
		}
		viewangles[YAW] = CL_Linestrafe(viewangles[YAW]);
	}
	else if (strafetype == Backpedal)
	{
		TasKeyUp(&in_forward);
		TasKeyUp(&in_moveright);
		TasKeyUp(&in_moveleft);
		TasKeyDown(&in_back);
		viewangles[YAW] = atan2(pmove->velocity[1], pmove->velocity[0]) * 180 / M_PI;
		float frac = viewangles[YAW] / M_U;
		frac -= trunc(frac);
		if (frac > 0.5)
			viewangles[YAW] += M_U;
		else if (frac < -0.5)
			viewangles[YAW] -= M_U;
	}
	viewangles[YAW] = anglemod(viewangles[YAW]);

	do_setyaw.do_it = false;
	do_olsshift.do_it = false;

	if (viewangles[PITCH] > cl_pitchdown->value)
		viewangles[PITCH] = cl_pitchdown->value;
	if (viewangles[PITCH] < -cl_pitchup->value)
		viewangles[PITCH] = -cl_pitchup->value;

	if (do_setpitch.do_it)
	{
		viewangles[PITCH] = do_setpitch.value;
		if (viewangles[PITCH] > cl_pitchdown->value)
			gEngfuncs.Cvar_SetValue("cl_pitchdown", viewangles[PITCH]);
		if (viewangles[PITCH] < -cl_pitchup->value)
			gEngfuncs.Cvar_SetValue("cl_pitchup", -viewangles[PITCH]);
		do_setpitch.do_it = false;
	}

	if (viewangles[ROLL] > 50)
		viewangles[ROLL] = 50;
	if (viewangles[ROLL] < -50)
		viewangles[ROLL] = -50;

	gEngfuncs.SetViewAngles( (float *)viewangles );

	if (strafetype == Nostrafe && old_strafetype != Nostrafe)
	{
		if (!(in_moveright.state & 32))
			TasKeyUp(&in_moveright);
		if (!(in_moveleft.state & 32))
			TasKeyUp(&in_moveleft);
		if (!(in_back.state & 32))
			TasKeyUp(&in_back);
	}
	in_moveright.state &= ~32;
	in_moveleft.state &= ~32;
	in_back.state &= ~32;
	old_strafetype = strafetype;

	cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_moveleft);
	cmd->forwardmove += cl_forwardspeed->value * CL_KeyState (&in_forward);
	cmd->forwardmove -= cl_backspeed->value * CL_KeyState (&in_back);
	cmd->upmove += cl_upspeed->value * CL_KeyState (&in_up);
	cmd->upmove -= cl_upspeed->value * CL_KeyState (&in_down);
}

bool CL_IsGroundEntBelow(float pos[3])
{
	float target[3] = {pos[0], pos[1], pos[2] - 2};
	pmtrace_t tr = pmove->PM_PlayerTrace(pos, target, PM_NORMAL, -1);
	return tr.plane.normal[2] >= 0.7;
}

/*
================
CL_CreateMove

Send the intended movement message to the server
if active == 1 then we are 1) not playing back demos ( where our commands are ignored ) and
2 ) we have finished signing on to server
================
*/
void CL_DLLEXPORT CL_CreateMove ( float frametime, struct usercmd_s *cmd, int active )
{	
//	RecClCL_CreateMove(frametime, cmd, active);

	float spd;
	vec3_t viewangles;
	static vec3_t oldangles;

	old_backspeed = cl_backspeed->value;
	old_sidespeed = cl_sidespeed->value;

	if ( active && !Bench_Active() )
	{
		memset (cmd, 0, sizeof(*cmd));
		g_bcap = gEngfuncs.pfnGetCvarFloat("sv_bcap") != 0;
		if (pmove->flags & FL_DUCKING)
			pmove->maxspeed *= 0.333;
		pmove->frametime = frametime;
		in_duck.state &= ~(8 + 16);
		in_jump.state &= ~(8 + 16);

		PM_CatagorizePosition();

		CL_AnglesAndMoves ( frametime, viewangles, cmd );

		// Allow mice and other controllers to add their inputs
		if (strafetype == Nostrafe)
			IN_Move ( frametime, cmd );
	}

	cmd->impulse = in_impulse;
	in_impulse = 0;

	cmd->weaponselect = g_weaponselect;
	g_weaponselect = 0;
	//
	// set button and flag bits
	//
	cmd->buttons = CL_ButtonBits( 1 );

	// If they're in a modal dialog, ignore the attack button.
	if(GetClientVoiceMgr()->IsInSquelchMode())
		cmd->buttons &= ~IN_ATTACK;

	gEngfuncs.GetViewAngles( (float *)viewangles );
	// Set current view angles.

	if ( g_iAlive )
	{
		VectorCopy( viewangles, cmd->viewangles );
		VectorCopy( viewangles, oldangles );
	}
	else
	{
		VectorCopy( oldangles, cmd->viewangles );
	}

	Bench_SetViewAngles( 1, (float *)&cmd->viewangles, frametime, cmd );

	cl_backspeed->value = old_backspeed;
	cl_sidespeed->value = old_sidespeed;
}

/*
============
CL_IsDead

Returns 1 if health is <= 0
============
*/
int	CL_IsDead( void )
{
	return ( gHUD.m_Health.m_iHealth <= 0 ) ? 1 : 0;
}

/*
============
CL_ButtonBits

Returns appropriate button info for keyboard and mouse state
Set bResetState to 1 to clear old state info
============
*/
int CL_ButtonBits( int bResetState )
{
	int bits = 0;

	if ( in_attack.state & 3 )
	{
		bits |= IN_ATTACK;
	}
	
	if (in_duck.state & (3 + 8) && !(in_duck.state & 16))
	{
		bits |= IN_DUCK;
	}
	if (in_jump.state & (3 + 8) && !(in_jump.state & 16))
	{
		bits |= IN_JUMP;
	}

	if ( in_forward.state & 3 )
	{
		bits |= IN_FORWARD;
	}
	
	if (in_back.state & 3)
	{
		bits |= IN_BACK;
	}

	if (in_use.state & 3)
	{
		bits |= IN_USE;
	}

	if (in_cancel)
	{
		bits |= IN_CANCEL;
	}

	if ( in_left.state & 3 )
	{
		bits |= IN_LEFT;
	}
	
	if (in_right.state & 3)
	{
		bits |= IN_RIGHT;
	}
	
	if ( in_moveleft.state & 3 )
	{
		bits |= IN_MOVELEFT;
	}
	
	if (in_moveright.state & 3)
	{
		bits |= IN_MOVERIGHT;
	}

	if (in_attack2.state & 3)
	{
		bits |= IN_ATTACK2;
	}

	if (in_reload.state & 3)
	{
		bits |= IN_RELOAD;
	}

	if (in_alt1.state & 3)
	{
		bits |= IN_ALT1;
	}

	if ( in_score.state & 3 )
	{
		bits |= IN_SCORE;
	}

	// Dead or in intermission? Shore scoreboard, too
	if ( CL_IsDead() || gHUD.m_iIntermission )
	{
		bits |= IN_SCORE;
	}

	if ( bResetState )
	{
		in_attack.state &= ~2;
		in_duck.state &= ~2;
		in_jump.state &= ~2;
		in_forward.state &= ~2;
		in_back.state &= ~2;
		in_use.state &= ~2;
		in_left.state &= ~2;
		in_right.state &= ~2;
		in_moveleft.state &= ~2;
		in_moveright.state &= ~2;
		in_attack2.state &= ~2;
		in_reload.state &= ~2;
		in_alt1.state &= ~2;
		in_score.state &= ~2;
	}

	return bits;
}

/*
============
CL_ResetButtonBits

============
*/
void CL_ResetButtonBits( int bits )
{
	int bitsNew = CL_ButtonBits( 0 ) ^ bits;

	// Has the attack button been changed
	if ( bitsNew & IN_ATTACK )
	{
		// Was it pressed? or let go?
		if ( bits & IN_ATTACK )
		{
			KeyDown( &in_attack );
		}
		else
		{
			// totally clear state
			in_attack.state &= ~7;
		}
	}
}

int MsgFunc_TasPlrInfo(const char *, int size, void *buf)
{
	BEGIN_READ(buf, size);
	pmove->velocity[0] = READ_FLOAT();
	pmove->velocity[1] = READ_FLOAT();
	pmove->velocity[2] = READ_FLOAT();
	pmove->origin[0] = READ_FLOAT();
	pmove->origin[1] = READ_FLOAT();
	pmove->origin[2] = READ_FLOAT();
	pmove->basevelocity[0] = READ_FLOAT();
	pmove->basevelocity[1] = READ_FLOAT();
	pmove->basevelocity[2] = READ_FLOAT();
	pmove->flags = READ_LONG();
	pmove->onground = (pmove->flags & FL_ONGROUND) ? 0 : -1;
	pmove->friction = READ_FLOAT();
	pmove->gravity = READ_FLOAT();
	pmove->waterlevel = READ_BYTE();
	pmove->bInDuck = READ_BYTE();

	return 1;
}

/*
============
InitInput
============
*/
void InitInput (void)
{
	gEngfuncs.pfnHookUserMsg("TasPlrInfo", MsgFunc_TasPlrInfo);
	gEngfuncs.pfnAddCommand("+linestrafe", IN_LinestrafeDown);
	gEngfuncs.pfnAddCommand("-linestrafe", IN_LinestrafeUp);
	gEngfuncs.pfnAddCommand("+backpedal", IN_BackpedalDown);
	gEngfuncs.pfnAddCommand("-backpedal", IN_BackpedalUp);
	gEngfuncs.pfnAddCommand("+leftstrafe", IN_LeftstrafeDown);
	gEngfuncs.pfnAddCommand("-leftstrafe", IN_LeftstrafeUp);
	gEngfuncs.pfnAddCommand("+rightstrafe", IN_RightstrafeDown);
	gEngfuncs.pfnAddCommand("-rightstrafe", IN_RightstrafeUp);
	gEngfuncs.pfnAddCommand("tas_pitch", IN_SetPitch);
	gEngfuncs.pfnAddCommand("tas_yaw", IN_SetYaw);
	gEngfuncs.pfnAddCommand("tas_olsshift", IN_OLSShift);
	gEngfuncs.pfnAddCommand("tas_cjmp", IN_ContJump);
	gEngfuncs.pfnAddCommand("tas_dtap", IN_DuckTap);
	gEngfuncs.pfnAddCommand("tas_dwj", IN_DuckWhenJump);
	gEngfuncs.pfnAddCommand("tas_lgagst", IN_LGAGST);
	gEngfuncs.pfnAddCommand("tas_jb", IN_JumpBug);
	gEngfuncs.pfnAddCommand("tas_db4c", IN_DuckB4Col);
	gEngfuncs.pfnAddCommand("tas_db4l", IN_DuckB4Land);

	gEngfuncs.pfnAddCommand ("+moveup",IN_UpDown);
	gEngfuncs.pfnAddCommand ("-moveup",IN_UpUp);
	gEngfuncs.pfnAddCommand ("+movedown",IN_DownDown);
	gEngfuncs.pfnAddCommand ("-movedown",IN_DownUp);
	gEngfuncs.pfnAddCommand ("+left",IN_LeftDown);
	gEngfuncs.pfnAddCommand ("-left",IN_LeftUp);
	gEngfuncs.pfnAddCommand ("+right",IN_RightDown);
	gEngfuncs.pfnAddCommand ("-right",IN_RightUp);
	gEngfuncs.pfnAddCommand ("+forward",IN_ForwardDown);
	gEngfuncs.pfnAddCommand ("-forward",IN_ForwardUp);
	gEngfuncs.pfnAddCommand ("+back",IN_BackDown);
	gEngfuncs.pfnAddCommand ("-back",IN_BackUp);
	gEngfuncs.pfnAddCommand ("+lookup", IN_LookupDown);
	gEngfuncs.pfnAddCommand ("-lookup", IN_LookupUp);
	gEngfuncs.pfnAddCommand ("+lookdown", IN_LookdownDown);
	gEngfuncs.pfnAddCommand ("-lookdown", IN_LookdownUp);
	gEngfuncs.pfnAddCommand ("+strafe", IN_StrafeDown);
	gEngfuncs.pfnAddCommand ("-strafe", IN_StrafeUp);
	gEngfuncs.pfnAddCommand ("+moveleft", IN_MoveleftDown);
	gEngfuncs.pfnAddCommand ("-moveleft", IN_MoveleftUp);
	gEngfuncs.pfnAddCommand ("+moveright", IN_MoverightDown);
	gEngfuncs.pfnAddCommand ("-moveright", IN_MoverightUp);
	gEngfuncs.pfnAddCommand ("+speed", IN_SpeedDown);
	gEngfuncs.pfnAddCommand ("-speed", IN_SpeedUp);
	gEngfuncs.pfnAddCommand ("+attack", IN_AttackDown);
	gEngfuncs.pfnAddCommand ("-attack", IN_AttackUp);
	gEngfuncs.pfnAddCommand ("+attack2", IN_Attack2Down);
	gEngfuncs.pfnAddCommand ("-attack2", IN_Attack2Up);
	gEngfuncs.pfnAddCommand ("+use", IN_UseDown);
	gEngfuncs.pfnAddCommand ("-use", IN_UseUp);
	gEngfuncs.pfnAddCommand ("+jump", IN_JumpDown);
	gEngfuncs.pfnAddCommand ("-jump", IN_JumpUp);
	gEngfuncs.pfnAddCommand ("impulse", IN_Impulse);
	gEngfuncs.pfnAddCommand ("+klook", IN_KLookDown);
	gEngfuncs.pfnAddCommand ("-klook", IN_KLookUp);
	gEngfuncs.pfnAddCommand ("+mlook", IN_MLookDown);
	gEngfuncs.pfnAddCommand ("-mlook", IN_MLookUp);
	gEngfuncs.pfnAddCommand ("+jlook", IN_JLookDown);
	gEngfuncs.pfnAddCommand ("-jlook", IN_JLookUp);
	gEngfuncs.pfnAddCommand ("+duck", IN_DuckDown);
	gEngfuncs.pfnAddCommand ("-duck", IN_DuckUp);
	gEngfuncs.pfnAddCommand ("+reload", IN_ReloadDown);
	gEngfuncs.pfnAddCommand ("-reload", IN_ReloadUp);
	gEngfuncs.pfnAddCommand ("+alt1", IN_Alt1Down);
	gEngfuncs.pfnAddCommand ("-alt1", IN_Alt1Up);
	gEngfuncs.pfnAddCommand ("+score", IN_ScoreDown);
	gEngfuncs.pfnAddCommand ("-score", IN_ScoreUp);
	gEngfuncs.pfnAddCommand ("+showscores", IN_ScoreDown);
	gEngfuncs.pfnAddCommand ("-showscores", IN_ScoreUp);
	gEngfuncs.pfnAddCommand ("+graph", IN_GraphDown);
	gEngfuncs.pfnAddCommand ("-graph", IN_GraphUp);
	gEngfuncs.pfnAddCommand ("+break",IN_BreakDown);
	gEngfuncs.pfnAddCommand ("-break",IN_BreakUp);

	lookstrafe			= gEngfuncs.pfnRegisterVariable ( "lookstrafe", "0", FCVAR_ARCHIVE );
	lookspring			= gEngfuncs.pfnRegisterVariable ( "lookspring", "0", FCVAR_ARCHIVE );
	cl_anglespeedkey	= gEngfuncs.pfnRegisterVariable ( "cl_anglespeedkey", "0.67", 0 );
	cl_yawspeed			= gEngfuncs.pfnRegisterVariable ( "cl_yawspeed", "210", 0 );
	cl_pitchspeed		= gEngfuncs.pfnRegisterVariable ( "cl_pitchspeed", "225", 0 );
	cl_upspeed			= gEngfuncs.pfnRegisterVariable ( "cl_upspeed", "320", 0 );
	cl_forwardspeed		= gEngfuncs.pfnRegisterVariable ( "cl_forwardspeed", "400", FCVAR_ARCHIVE );
	cl_backspeed		= gEngfuncs.pfnRegisterVariable ( "cl_backspeed", "400", FCVAR_ARCHIVE );
	cl_sidespeed		= gEngfuncs.pfnRegisterVariable ( "cl_sidespeed", "400", 0 );
	cl_movespeedkey		= gEngfuncs.pfnRegisterVariable ( "cl_movespeedkey", "0.3", 0 );
	cl_pitchup			= gEngfuncs.pfnRegisterVariable ( "cl_pitchup", "89", 0 );
	cl_pitchdown		= gEngfuncs.pfnRegisterVariable ( "cl_pitchdown", "89", 0 );

	cl_vsmoothing		= gEngfuncs.pfnRegisterVariable ( "cl_vsmoothing", "0.05", FCVAR_ARCHIVE );

	m_pitch			    = gEngfuncs.pfnRegisterVariable ( "m_pitch","0.022", FCVAR_ARCHIVE );
	m_yaw				= gEngfuncs.pfnRegisterVariable ( "m_yaw","0.022", FCVAR_ARCHIVE );
	m_forward			= gEngfuncs.pfnRegisterVariable ( "m_forward","1", FCVAR_ARCHIVE );
	m_side				= gEngfuncs.pfnRegisterVariable ( "m_side","0.8", FCVAR_ARCHIVE );

	cl_mtype = gEngfuncs.pfnRegisterVariable("cl_mtype", "1", 0);
	cl_db4c_ceil = gEngfuncs.pfnRegisterVariable("cl_db4c_ceil", "0", 0);

	// Initialize third person camera controls.
	CAM_Init();
	// Initialize inputs
	IN_Init();
	// Initialize keyboard
	KB_Init();
	// Initialize view system
	V_Init();
}

/*
============
ShutdownInput
============
*/
void ShutdownInput (void)
{
	IN_Shutdown();
	KB_Shutdown();
}

#include "interface.h"
void CL_UnloadParticleMan( void );

#if defined( _TFC )
void ClearEventList( void );
#endif

void CL_DLLEXPORT HUD_Shutdown( void )
{
//	RecClShutdown();

	ShutdownInput();

#if defined( _TFC )
	ClearEventList();
#endif
	
	CL_UnloadParticleMan();
}
