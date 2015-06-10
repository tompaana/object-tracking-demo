using System;
using System.Collections.Generic;
using System.Text;

namespace ObjectTrackingDemo
{
    public enum VideoEffectState : int
    {
        Idle = 0,
        Locking = 1,
        Locked = 2,
        Triggered = 3,
        PostProcess = 4
    };

    public class StateManager
    {
        public delegate void StateChangedDelegate(VideoEffectState newState, VideoEffectState oldState);
        public event StateChangedDelegate StateChanged;

        private VideoEffectState _state = VideoEffectState.Idle;
        public VideoEffectState State
        {
            get
            {
                return _state;
            }
            set
            {
                SetState(value);
            }
        }

        public StateManager()
        {
        }

        /// <summary>
        /// For convenience
        /// </summary>
        /// <param name="newState"></param>
        public void SetState(int newState)
        {
            State = (VideoEffectState)newState;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="newState"></param>
        private void SetState(VideoEffectState newState)
        {
            VideoEffectState oldState = _state;

            bool changed = false;

            switch (newState)
            {
                case VideoEffectState.Idle: // We can always go to idle
                    if (_state != VideoEffectState.Idle)
                    {
                        _state = VideoEffectState.Idle;
                        changed = true;
                    }

                    break;

                case VideoEffectState.Locking: // From Idle -> Locking and Locking <- Lock is accepted
                    if (_state == VideoEffectState.Idle
                        || _state == VideoEffectState.Locked)
                    {
                        
                        _state = VideoEffectState.Locking;
                        changed = true;
                    }

                    break;

                case VideoEffectState.Locked: // From Locking -> Locked is accepted
                    if (_state == VideoEffectState.Locking)
                    {
                        _state = VideoEffectState.Locked;
                        changed = true;
                    }

                    break;

                case VideoEffectState.Triggered: // From Locked -> Triggered is accepted
                    if (_state == VideoEffectState.Locked)
                    {
                        _state = VideoEffectState.Triggered;
                        changed = true;
                    }

                    break;

                case VideoEffectState.PostProcess: // From Triggered -> PostProcess
                    if (_state == VideoEffectState.Triggered)
                    {
                        _state = VideoEffectState.PostProcess;
                        changed = true;
                    }

                    break;
                default:
                    break;
            }

            if (StateChanged != null && changed)
            {
                StateChanged(_state, oldState);
            }

            System.Diagnostics.Debug.WriteLine("State changed: " + oldState + " -> " + newState);
        }
    }
}
