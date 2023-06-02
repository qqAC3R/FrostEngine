using System;
using Frost;

namespace Frost
{
    public class ActorCamera : Entity
    {
        public Entity ActorEntity = null;
        public Vector3 m_CameraOffset = new Vector3(0, 1, 3);

        protected override void OnCreate()
        {


        }
        protected override void OnUpdate(float deltaTime)
        {
            this.Translation = ActorEntity.Translation + m_CameraOffset;
            //this.Rotation = new Vector3(this.Rotation.X, 0.0f, this.Rotation.Z);
        }

        protected override void OnDestroy()
        {
        }

    }
}