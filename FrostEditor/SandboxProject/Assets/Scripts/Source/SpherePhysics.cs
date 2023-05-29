using System;
using Frost;

namespace Frost
{
    public class MeshPlayer : Entity
    {
        public uint IsConvex = 0;

        protected override void OnCreate()
        {
            //AddTriggerBeginCallback(delegate (Entity e)
            //{
            //    Console.WriteLine($"Begin trigger with {e.ID}");
            //});
            //
            //AddTriggerEndCallback(delegate (Entity e)
            //{
            //    Console.WriteLine($"End trigger with {e.ID}");
            //});

            //GetComponent<BoxColliderComponent>().IsTrigger = true;
            Log.Critical("Nigga");
        }

        protected override void OnUpdate(float deltaTime)
        {
            //if(IsConvex == 1)
            //{
            //    GetComponent<MeshColliderComponent>().IsConvex = true;
            //}
            Log.Critical("Nigga");
            Log.Critical("Nigga3");
            Log.Critical("N2gga3");
        }
    }
}
