//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:  Sound channel list
//
//-----------------------------------------------------------------------------


#include <assert.h>
#include "i_sound.h"
#include "i_channellist.h"


//==========================================================================
//
// S_GetChannel
//
// Returns a free channel for the system sound interface.
//
//==========================================================================

FISoundChannel *FIChannelList::GetChannel(void *syschan)
{
	FISoundChannel *chan;
	
	if (FreeChannels != nullptr)
	{
		chan = FreeChannels;
		UnlinkChannel(chan);
		return chan;
	}
	else
	{
		chan = (FISoundChannel *)malloc(ChannelStructSize);
	}
	memset(chan, 0, ChannelStructSize);
	LinkChannel(chan, &Channels);
	chan->SysChannel = syschan;
	return chan;
}

//==========================================================================
//
// S_ReturnChannel
//
// Returns a channel to the free pool.
//
//==========================================================================

void FIChannelList::ReturnChannel(FISoundChannel *chan)
{
	UnlinkChannel(chan);
	LinkChannel(chan, &FreeChannels);
}

//==========================================================================
//
// S_UnlinkChannel
//
//==========================================================================

void FIChannelList::UnlinkChannel(FISoundChannel *chan)
{
	*(chan->PrevChan) = chan->NextChan;
	if (chan->NextChan != nullptr)
	{
		chan->NextChan->PrevChan = chan->PrevChan;
	}
}

//==========================================================================
//
// S_LinkChannel
//
//==========================================================================

void FIChannelList::LinkChannel(FISoundChannel *chan, FISoundChannel **head)
{
	chan->NextChan = *head;
	if (chan->NextChan != nullptr)
	{
		chan->NextChan->PrevChan = &chan->NextChan;
	}
	*head = chan;
	chan->PrevChan = head;
}

//==========================================================================
//
//
//
//==========================================================================

void FIChannelList::ReturnAllChannels()
{
	while (Channels != nullptr)
	{
		ReturnChannel(Channels);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FIChannelList::StopAllChannels()
{
	auto chan = Channels;
	while (chan != nullptr)
	{
		auto next = chan->NextChan;
		StopChannel(chan);
		chan = next;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FIChannelList::FreeChannelList()
{
	// This should only be called when all sounds have been stopped.
	assert(Channels == nullptr);
	for(auto p = FreeChannels; p; p = p->NextChan)
	{
		free(p);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FIChannelList::SetPitch(FISoundChannel *chan, float pitch)
{
	GSnd->ChannelPitch(chan, std::max(0.0001f, pitch));
	chan->Pitch = std::max(1, int(float(NORM_PITCH) * pitch));
}


//==========================================================================
//
// S_ChannelEnded (callback for sound interface code)
//
//==========================================================================

void FIChannelList::ChannelEnded(FISoundChannel *schan)
{
	bool evicted;
	
	if (schan != NULL)
	{
		// If the sound was stopped with GSnd->StopSound(), then we know
		// it wasn't evicted. Otherwise, if it's looping, it must have
		// been evicted. If it's not looping, then it was evicted if it
		// didn't reach the end of its playback.
		if (schan->ChanFlags & CHAN_FORGETTABLE)
		{
			evicted = false;
		}
		else if (schan->ChanFlags & (CHAN_LOOP | CHAN_EVICTED))
		{
			evicted = true;
		}
		else
		{
			unsigned int pos = GSnd->GetPosition(schan);
			unsigned int len = GSnd->GetSampleLength(SoundData(schan));
			if (pos == 0)
			{
				evicted = !!(schan->ChanFlags & CHAN_JUSTSTARTED);
			}
			else
			{
				evicted = (pos < len);
			}
		}
		if (!evicted)
		{
			ReturnChannel(schan);
		}
		else
		{
			schan->ChanFlags |= CHAN_EVICTED;
			schan->SysChannel = nullptr;
		}
	}
}

//==========================================================================
//
// S_ChannelVirtualChanged (callback for sound interface code)
//
//==========================================================================

void FIChannelList::ChannelVirtualChanged(FISoundChannel *schan, bool is_virtual)
{
	if (is_virtual)
	{
		schan->ChanFlags |= CHAN_VIRTUAL;
	}
	else
	{
		schan->ChanFlags &= ~CHAN_VIRTUAL;
	}
}

//==========================================================================
//
// S_StopChannel
//
//==========================================================================

void FIChannelList::StopChannel(FISoundChannel *chan)
{
	if (chan == NULL)
		return;
	
	if (chan->SysChannel != NULL)
	{
		// S_EvictAllChannels() will set the CHAN_EVICTED flag to indicate
		// that it wants to keep all the channel information around.
		if (!(chan->ChanFlags & CHAN_EVICTED))
		{
			chan->ChanFlags |= CHAN_FORGETTABLE;
			ChannelStopped(chan);

		}
		GSnd->StopChannel(chan);
	}
	else
	{
		ReturnChannel(chan);
	}
}


//==========================================================================
//
// S_EvictAllChannels
//
// Forcibly evicts all channels so that there are none playing, but all
// information needed to restart them is retained.
//
//==========================================================================

void FIChannelList::EvictAllChannels()
{
	FISoundChannel *chan, *next;
	
	for (chan = Channels; chan != nullptr; chan = next)
	{
		next = chan->NextChan;
		
		if (!(chan->ChanFlags & CHAN_EVICTED))
		{
			chan->ChanFlags |= CHAN_EVICTED;
			if (chan->SysChannel != nullptr)
			{
				if (!(chan->ChanFlags & CHAN_ABSTIME))
				{
					chan->StartTime = GSnd ? GSnd->GetPosition(chan) : 0;
					chan->ChanFlags |= CHAN_ABSTIME;
				}
				StopChannel(chan);
			}
		}
	}
}

//==========================================================================
//
// S_RestoreEvictedChannel
//
// Recursive helper for S_RestoreEvictedChannels().
//
//==========================================================================

void FIChannelList::RestoreEvictedChannel(FISoundChannel *chan)
{
	if (chan == NULL)
	{
		return;
	}
	RestoreEvictedChannel(chan->NextChan);
	if (chan->ChanFlags & CHAN_EVICTED)
	{
		RestartSound(chan);
		if (!(chan->ChanFlags & CHAN_LOOP))
		{
			if (chan->ChanFlags & CHAN_EVICTED)
			{ // Still evicted and not looping? Forget about it.
				ReturnChannel(chan);
			}
			else if (!(chan->ChanFlags & CHAN_JUSTSTARTED))
			{ // Should this sound become evicted again, it's okay to forget about it.
				chan->ChanFlags |= CHAN_FORGETTABLE;
			}
		}
	}
	else if (chan->SysChannel == NULL && (chan->ChanFlags & (CHAN_FORGETTABLE | CHAN_LOOP)) == CHAN_FORGETTABLE)
	{
		ReturnChannel(chan);
	}
}

//==========================================================================
//
// S_RestoreEvictedChannels
//
// Restarts as many evicted channels as possible. Any channels that could
// not be started and are not looping are moved to the free pool.
//
//==========================================================================

void FIChannelList::RestoreEvictedChannels()
{
	// Restart channels in the same order they were originally played.
	RestoreEvictedChannel(Channels);
}

