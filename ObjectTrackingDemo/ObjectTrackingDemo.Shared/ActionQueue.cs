using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;

namespace ObjectTrackingDemo
{
    public class ActionQueue : IDisposable
    {
        private Timer _executeTimer;

        public enum VideoEffectState
        {
            Idle = 0,
            Executing,
            Queued
        };

        private List<Action> _actionList;

        public VideoEffectState State
        {
            get;
            private set;
        }

        private int _executeIntervalInMilliseconds = 0;
        public int ExecuteIntervalInMilliseconds
        {
            get
            {
                return _executeIntervalInMilliseconds;
            }
            set
            {
                if (_executeTimer != null)
                {
                    _executeTimer.Dispose();
                    _executeTimer = null;
                }

                _executeIntervalInMilliseconds = value;

                if (_executeIntervalInMilliseconds > 0)
                {
                    _executeTimer = new Timer(ExecuteLastAction, null, 0, _executeIntervalInMilliseconds);
                }
            }
        }

        public ActionQueue()
        {
            _actionList = new List<Action>();
        }

        public void Execute(Action action)
        {
            _actionList.Add(action);

            switch (State)
            {
                case VideoEffectState.Idle:
                case VideoEffectState.Queued: // State machine transition: Idle/Queued -> Executing
                    if (ExecuteIntervalInMilliseconds == 0)
                    {
                        ExecuteLastAction();
                    }

                    break;
                case VideoEffectState.Executing: // State machine transition: Executing -> Queued
                    State = VideoEffectState.Queued;
                    break;
                default:
                    // Do nothing
                    break;
            }
        }

        private void ExecuteLastAction(object state = null)
        {
            if (_actionList.Count > 0)
            {
                State = VideoEffectState.Executing;

                // Execute the action, which was added last and ignore the rest
                Action lastAction = _actionList[_actionList.Count - 1];
                _actionList.Clear();
                lastAction();

                switch (State)
                {
                    case VideoEffectState.Executing:
                        State = VideoEffectState.Idle;
                        break;
                    case VideoEffectState.Queued:
                        if (ExecuteIntervalInMilliseconds == 0)
                        {
                            // Execute next action immediately
                            ExecuteLastAction();
                        }

                        break;
                    default:
                        // Do nothing
                        break;
                }
            }
            else
            {
                State = VideoEffectState.Idle;
            }
        }

        public void Dispose()
        {
            if (_executeTimer != null)
            {
                _executeTimer.Dispose();
                _executeTimer = null;
            }
        }
    }
}
