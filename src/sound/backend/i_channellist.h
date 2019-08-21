#pragma once

#include <stdint.h>
#include <type_traits>

// Note that at the moment these are shared with the sound playing interface. This should probably be changed!
enum ESoundChannelFlags
{
	// modifier flags
	CHAN_LISTENERZ			= 8,
	CHAN_MAYBE_LOCAL		= 16,
	CHAN_UI					= 32,	// Do not record sound in savegames.
	CHAN_NOPAUSE			= 64,	// Do not pause this sound in menus.
	CHAN_AREA				= 128,	// Sound plays from all around. Only valid with sector sounds.
	CHAN_LOOP				= 256,
	
	CHAN_IS3D				= 1,	// internal: Sound is 3D.
	CHAN_EVICTED			= 2,	// internal: Sound was evicted.
	CHAN_FORGETTABLE		= 4,	// internal: Forget channel data when sound stops.
	CHAN_JUSTSTARTED		= 512,	// internal: Sound has not been updated yet.
	CHAN_ABSTIME			= 1024, // internal: Start time is absolute and does not depend on current time.
	CHAN_VIRTUAL			= 2048, // internal: Channel is currently virtual
};

// Default rolloff information.
struct FRolloffInfo
{
	int RolloffType;
	float MinDistance;
	union { float MaxDistance; float RolloffFactor; };
};

struct FISoundChannel
{
	void		*SysChannel;	// Channel information from the system interface.
	FISoundChannel	*NextChan;	// Next channel in this list.
	FISoundChannel **PrevChan;	// Previous channel in this list.
	
	uint64_t	StartTime;		// Sound start time in DSP clocks.
	
	// The sound interface doesn't use these directly but it needs to pass them to a
	// callback that can't be passed a sound channel pointer
	FRolloffInfo Rolloff;
	float		DistanceScale;
	float		DistanceSqr;
	float		Volume;
	int16_t		Pitch;		// Pitch variation.
	int8_t		Priority;
	bool		ManualRolloff;
	int			ChanFlags;
};


class FIChannelList
{
protected:
	const size_t ChannelStructSize;
	FISoundChannel *Channels = nullptr;
	FISoundChannel *FreeChannels = nullptr;
	
	FIChannelList(size_t size) : ChannelStructSize(size) {}
	void LinkChannel(FISoundChannel *chan, FISoundChannel **list);
	
public:
	
	enum
	{
		NORM_PITCH = 128
	};
	
	FISoundChannel *GetChannel(void *syschan);
	void ReturnChannel(FISoundChannel *chan);
	void UnlinkChannel(FISoundChannel *chan);
	void FreeChannelList();
	void ChannelEnded(FISoundChannel *schan);
	void ChannelVirtualChanged(FISoundChannel *ichan, bool is_virtual);
	void StopChannel(FISoundChannel *chan);
	void EvictAllChannels();
	void RestoreEvictedChannel(FISoundChannel *chan);
	void RestoreEvictedChannels();
	void ReturnAllChannels();
	void StopAllChannels();
	void SetPitch(FISoundChannel *chan, float dpitch);

	FISoundChannel *FirstChannel() const
	{
		return Channels;
	}



	virtual SoundHandle &SoundData(FISoundChannel *chan) = 0;	// Only the implementor knows this.
	virtual void ChannelStopped(FISoundChannel *chan) {}	// Give the implementor a chance for custom, actions; optional.
	virtual void RestartSound(FISoundChannel *chan) = 0;	// Only the implementor can do this.

	~FIChannelList() { FreeChannelList(); }
};
